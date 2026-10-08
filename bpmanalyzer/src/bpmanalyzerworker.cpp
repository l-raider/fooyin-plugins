/*
 * Fooyin BPM Analyzer Plugin
 * Copyright © 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bpmanalyzerworker.h"

#include "bpmanalyzercancellation.h"
#include "bpmanalyzerdefs.h"

#include <core/constants.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>
#include <utils/scopeguard.h>

#include <BPMDetect.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace Fooyin::BpmAnalyzer {

namespace {

// ---------------------------------------------------------------------------
// SoundTouch sample type
// BPMDetect::inputSamples expects interleaved samples of SoundTouch's
// SAMPLETYPE: float with the default float build, 16-bit signed when built
// with SOUNDTOUCH_INTEGER_SAMPLES. We match that at compile time.
// ---------------------------------------------------------------------------
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
constexpr SampleFormat kSoundTouchSampleFormat = SampleFormat::S16;
using SoundTouchSample = short;
#else
constexpr SampleFormat kSoundTouchSampleFormat = SampleFormat::F32;
using SoundTouchSample = float;
#endif

// Converts one AudioBuffer to interleaved SoundTouch samples, preserving the
// input channel count: BPMDetect performs the mono mix-down itself when fed
// multi-channel samples. Returns empty optional on conversion failure, empty
// vector on zero-frame input.
std::optional<std::vector<SoundTouchSample>>
convertToSoundTouchSamples(const AudioBuffer& buf)
{
    const int inFrames = buf.frameCount();
    if(inFrames <= 0)
        return std::vector<SoundTouchSample>{};

    AudioFormat targetFormat = buf.format();
    targetFormat.setSampleFormat(kSoundTouchSampleFormat);

    const AudioBuffer converted = Audio::convert(buf, targetFormat);
    if(!converted.isValid() || converted.byteCount() == 0)
        return {};

    const auto* data = reinterpret_cast<const SoundTouchSample*>(converted.data());
    const auto count
        = static_cast<size_t>(converted.byteCount()) / sizeof(SoundTouchSample);
    return std::vector<SoundTouchSample>{data, data + count};
}

// ---------------------------------------------------------------------------
// BPM aggregation from SoundTouch beat positions
// ---------------------------------------------------------------------------

constexpr float kMinBpm = 45.0f;
constexpr float kMaxBpm = 190.0f;

//! CD audio sector size in frames (1/75 s at 44100 Hz), matching AccurateRip.
constexpr uint64_t FramesPerSector = 588;

std::optional<uint64_t> cueSectorProperty(const Track& track, const char* name)
{
    const auto properties   = track.extraProperties();
    const auto* const value = properties.find(QString::fromLatin1(name));
    if(!value) {
        return {};
    }

    bool ok{false};
    const uint64_t sector = value->toULongLong(&ok);
    return ok ? std::optional<uint64_t>{sector} : std::optional<uint64_t>{};
}

uint64_t framesForDuration(uint64_t durationMs, int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    const auto rate = static_cast<uint64_t>(sampleRate);
    if(durationMs > std::numeric_limits<uint64_t>::max() / rate) {
        return std::numeric_limits<uint64_t>::max();
    }

    return durationMs * rate / 1000;
}

AudioBuffer trimBuffer(const AudioBuffer& buffer, uint64_t frames)
{
    if(!buffer.isValid() || std::cmp_greater_equal(frames, buffer.frameCount())) {
        return buffer;
    }

    const auto bytes = static_cast<size_t>(buffer.format().bytesForFrames(static_cast<int>(frames)));
    return {buffer.constData().first(bytes), buffer.format(), buffer.startTime()};
}

AudioBuffer removeLeadingFrames(const AudioBuffer& buffer, uint64_t frames)
{
    if(!buffer.isValid() || frames == 0) {
        return buffer;
    }

    if(std::cmp_greater_equal(frames, buffer.frameCount())) {
        return {};
    }

    const size_t offset = static_cast<size_t>(buffer.format().bytesForFrames(static_cast<int>(frames)));
    return {buffer.constData().subspan(offset), buffer.format(), buffer.startTime()};
}

struct BpmCandidate
{
    float bpm;
    float weight;
};

// Derives BPM candidates from inter-beat intervals.
// Beat positions (in seconds) come from BPMDetect::getBeats().
// Weight = mean strength of the two adjacent beats bracketing each interval.
std::vector<BpmCandidate> buildCandidates(soundtouch::BPMDetect& detector)
{
    // First query: how many beats were collected?
    const int maxBeats = detector.getBeats(nullptr, nullptr, 0);
    if(maxBeats < 2)
        return {};

    std::vector<float> pos(static_cast<size_t>(maxBeats));
    std::vector<float> strength(static_cast<size_t>(maxBeats));
    const int numBeats = detector.getBeats(pos.data(), strength.data(), maxBeats);
    if(numBeats < 2)
        return {};

    // Sort by ascending position
    std::vector<int> idx(static_cast<size_t>(numBeats));
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b) { return pos[static_cast<size_t>(a)] < pos[static_cast<size_t>(b)]; });

    std::vector<BpmCandidate> candidates;
    candidates.reserve(static_cast<size_t>(numBeats - 1));

    for(int i = 0; i + 1 < numBeats; ++i) {
        const int ia = idx[static_cast<size_t>(i)];
        const int ib = idx[static_cast<size_t>(i + 1)];
        const float dt = pos[static_cast<size_t>(ib)] - pos[static_cast<size_t>(ia)];
        if(dt <= 0.0f)
            continue;

        const float bpm = 60.0f / dt;
        if(bpm < kMinBpm || bpm > kMaxBpm)
            continue;

        const float w = 0.5f * (strength[static_cast<size_t>(ia)] + strength[static_cast<size_t>(ib)]);
        if(w <= 0.0f)
            continue;

        candidates.push_back({bpm, w});
    }

    return candidates;
}

float applyAggregation(const std::vector<BpmCandidate>& candidates,
                       float fallbackBpm,
                       AggregationMethod method)
{
    if(candidates.empty())
        return fallbackBpm;

    switch(method) {
        case AggregationMethod::WeightedAverage: {
            double sumWB = 0.0, sumW = 0.0;
            for(const auto& c : candidates) {
                sumWB += static_cast<double>(c.bpm) * static_cast<double>(c.weight);
                sumW  += static_cast<double>(c.weight);
            }
            // sumW is always > 0: buildCandidates filters w <= 0,
            // and applyAggregation is only called with non-empty candidates.
            return static_cast<float>(sumWB / sumW);
        }

        case AggregationMethod::Mean: {
            double sum = 0.0;
            for(const auto& c : candidates)
                sum += static_cast<double>(c.bpm);
            return static_cast<float>(sum / static_cast<double>(candidates.size()));
        }

        case AggregationMethod::Median: {
            std::vector<float> bpms;
            bpms.reserve(candidates.size());
            for(const auto& c : candidates)
                bpms.push_back(c.bpm);
            std::sort(bpms.begin(), bpms.end());
            const size_t mid = bpms.size() / 2;
            if(bpms.size() % 2 == 0)
                return 0.5f * (bpms[mid - 1] + bpms[mid]);
            return bpms[mid];
        }

        case AggregationMethod::Mode: {
            // Bin by rounded integer BPM; pick the bin with the highest total weight
            std::map<int, float> bins;
            for(const auto& c : candidates)
                bins[static_cast<int>(std::round(c.bpm))] += c.weight;

            const auto best = std::max_element(
                bins.cbegin(), bins.cend(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            return static_cast<float>(best->first);
        }
    }

    return fallbackBpm;
}

} // namespace

// ---------------------------------------------------------------------------
// BpmAnalyzerWorker
// ---------------------------------------------------------------------------

BpmAnalyzerWorker::BpmAnalyzerWorker(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

BpmResult BpmAnalyzerWorker::computeBpm(const Track& track,
                                        CancellationToken& token) const
{
    // ---- Read settings ----
    const BpmAnalyzerSettings settings = BpmAnalyzerSettings::load();
    const auto method       = static_cast<AggregationMethod>(settings.aggregationMethod);
    const int sampleLength  = settings.sampleLength;
    const bool skipExisting = settings.skipExisting;
    const int precision     = settings.bpmPrecision;

    // ---- Build result stub ----
    BpmResult result;
    result.track = track;

    if(track.isRemote()) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Remote streams cannot be analyzed");
        return result;
    }

    // Use fooyin's canonical editability check so CUE-bounded tracks (whose
    // metadata belongs to the shared container file) are never written.
    result.writable = track.isMetadataEditable(m_audioLoader->canWriteMetadata(track));

    const QStringList existingBpm = track.extraTag(QLatin1String{BpmTagField});
    if(!existingBpm.isEmpty())
        result.storedBpm = existingBpm.first();

    if(skipExisting && !result.storedBpm.isEmpty()) {
        result.status = BpmResult::Status::Skipped;
        return result;
    }

    // ---- Open decoder ----
    // ForConversion selects the offline source policy and NoLooping disables
    // all loop/repeat behaviour for the single pass.
    const auto loaded = m_audioLoader->loadDecoderForTrack(
        track,
        AudioDecoder::NoLooping | AudioDecoder::ForConversion);

    if(!loaded.decoder || !loaded.format) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("No decoder available");
        return result;
    }

    const AudioFormat fmt = *loaded.format;
    if(!fmt.isValid()) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Could not determine audio format");
        return result;
    }

    const int sampleRate = fmt.sampleRate();
    const int inputChans = fmt.channelCount();
    if(sampleRate <= 0 || inputChans <= 0) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Invalid audio format (zero rate or channels)");
        return result;
    }

    const auto cueStartSector = cueSectorProperty(track, Constants::CueIndex01Sector);
    const auto cueEndSector   = cueSectorProperty(track, Constants::CueEndSector);

    if(cueStartSector && cueEndSector && *cueEndSector <= *cueStartSector) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Invalid CUE track boundaries");
        return result;
    }

    AudioDecoder* const decoder = loaded.decoder.get();
    decoder->start();
    token.registerDecoder(decoder);
    const auto stopDecoder = scopeGuard([&token, decoder] {
        token.unregisterDecoder(decoder);
        decoder->stop();
    });

    // Honour segment boundaries: seek to the segment offset unless the track
    // is CUE-bounded, in which case the sector properties define the region.
    if(track.offset() > 0 && !cueStartSector) {
        if(!decoder->isSeekable()) {
            result.status      = BpmResult::Status::Error;
            result.errorString = QObject::tr("Decoder cannot seek to the track segment");
            return result;
        }
        decoder->seek(track.offset());
    }

    uint64_t framesToSkip = cueStartSector.value_or(0) * FramesPerSector;
    std::optional<uint64_t> framesRemaining;

    if(cueStartSector && cueEndSector) {
        framesRemaining = (*cueEndSector - *cueStartSector) * FramesPerSector;

        // BPM analysis tolerates approximate alignment, so avoid decoding and
        // discarding the whole file prefix for every CUE track by seeking to
        // the segment start when the decoder supports it. The sector -> ms
        // conversion assumes CD audio (588 frames/sector at the decoded rate).
        if(decoder->isSeekable()) {
            const uint64_t startMs
                = *cueStartSector * FramesPerSector * 1000 / static_cast<uint64_t>(sampleRate);
            decoder->seek(startMs);
            framesToSkip = 0;
        }
    }
    else if(track.isBoundedSegment() && track.duration() > 0 && !cueStartSector) {
        framesRemaining = framesForDuration(track.duration(), sampleRate);
    }

    // ---- Initialise BPMDetect ----
    // Feed the full interleaved channel data; BPMDetect mixes down to mono
    // internally via its decimator, so no separate mono conversion is needed.
    soundtouch::BPMDetect detector(inputChans, sampleRate);

    // ---- Decode and feed samples ----
    const qint64 maxFrames       = static_cast<qint64>(sampleLength) * sampleRate;
    qint64       framesProcessed = 0;

    constexpr size_t ChunkBytes = 65536;
    while(!token.isCancelled() && framesProcessed < maxFrames) {
        auto read = decoder->readAudio(ChunkBytes);

        if(read.status == AudioDecoder::ReadStatus::NeedMoreInput) {
            continue;
        }

        if(read.status == AudioDecoder::ReadStatus::Error) {
            result.status      = BpmResult::Status::Error;
            result.errorString = read.error.isEmpty() ? QObject::tr("Decoder error") : read.error;
            return result;
        }

        if(read.status == AudioDecoder::ReadStatus::EndOfStream) {
            if(framesToSkip > 0 || (framesRemaining && *framesRemaining > 0)) {
                result.status      = BpmResult::Status::Error;
                result.errorString = QObject::tr("Decoder ended before the track segment was complete");
                return result;
            }
            break;
        }

        AudioBuffer buf = std::move(read.buffer);
        if(!buf.isValid()) {
            result.status      = BpmResult::Status::Error;
            result.errorString = QObject::tr("Decoder returned invalid audio");
            return result;
        }

        if(framesToSkip > 0) {
            const uint64_t skipped
                = std::min<uint64_t>(framesToSkip, static_cast<uint64_t>(buf.frameCount()));
            buf = removeLeadingFrames(buf, skipped);
            framesToSkip -= skipped;
            if(!buf.isValid()) {
                continue;
            }
        }

        if(framesRemaining) {
            buf = trimBuffer(buf, *framesRemaining);
            *framesRemaining -= static_cast<uint64_t>(buf.frameCount());
        }

        auto samples = convertToSoundTouchSamples(buf);
        if(!samples) {
            result.status      = BpmResult::Status::Error;
            result.errorString = QObject::tr("Audio format conversion failed");
            return result;
        }

        if(!samples->empty()) {
            // inputSamples' count is in *frames*; each frame holds `inputChans`
            // interleaved values. Passing the sample count would make SoundTouch
            // read `channels` values per frame and walk past the buffer.
            const auto frameCount = static_cast<int>(samples->size() / inputChans);
            detector.inputSamples(samples->data(), frameCount);
        }

        framesProcessed += buf.frameCount();

        if(framesRemaining && *framesRemaining == 0) {
            break;
        }
    }

    if(token.isCancelled()) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Cancelled");
        return result;
    }

    // ---- Extract BPM ----
    // getBpm() is the direct autocorrelation result; used as fallback.
    const float directBpm = detector.getBpm();

    // Build inter-beat BPM candidates for configurable aggregation.
    const auto candidates = buildCandidates(detector);
    const float bpm = applyAggregation(candidates, directBpm, method);

    if(bpm < kMinBpm || bpm > kMaxBpm) {
        // Detection failed — track may be too short or have no clear beat
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("BPM detection failed (no clear beat found)");
        return result;
    }

    result.analyzedBpm = formatBpmValue(bpm, precision);
    result.status = result.storedBpm.isEmpty() ? BpmResult::Status::New
                                               : BpmResult::Status::Updated;
    return result;
}

} // namespace Fooyin::BpmAnalyzer
