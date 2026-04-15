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

#include "bpmanalyzerdefs.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>

#include <BPMDetect.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

extern "C"
{
#include <libavcodec/version.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

using namespace Qt::StringLiterals;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))

namespace Fooyin::BpmAnalyzer {

namespace {

// ---------------------------------------------------------------------------
// SoundTouch SAMPLETYPE detection
// BPMDetect::inputSamples expects either short (integer build) or float
// (float build, default). We pick the matching AV_SAMPLE_FMT at compile time.
// ---------------------------------------------------------------------------
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
constexpr AVSampleFormat kSoundTouchAvFmt = AV_SAMPLE_FMT_S16;
using SoundTouchSample = short;
#else
constexpr AVSampleFormat kSoundTouchAvFmt = AV_SAMPLE_FMT_FLT;
using SoundTouchSample = float;
#endif

// ---------------------------------------------------------------------------
// Channel layout helpers — adapted from audiochecksum (same ffmpeg compat shim)
// ---------------------------------------------------------------------------

uint64_t channelBit(const AudioFormat::ChannelPosition position)
{
    using P = AudioFormat::ChannelPosition;
    switch(position) {
        case P::FrontLeft:          return static_cast<uint64_t>(AV_CH_FRONT_LEFT);
        case P::FrontRight:         return static_cast<uint64_t>(AV_CH_FRONT_RIGHT);
        case P::FrontCenter:        return static_cast<uint64_t>(AV_CH_FRONT_CENTER);
        case P::LFE:                return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY);
        case P::BackLeft:           return static_cast<uint64_t>(AV_CH_BACK_LEFT);
        case P::BackRight:          return static_cast<uint64_t>(AV_CH_BACK_RIGHT);
        case P::FrontLeftOfCenter:  return static_cast<uint64_t>(AV_CH_FRONT_LEFT_OF_CENTER);
        case P::FrontRightOfCenter: return static_cast<uint64_t>(AV_CH_FRONT_RIGHT_OF_CENTER);
        case P::BackCenter:         return static_cast<uint64_t>(AV_CH_BACK_CENTER);
        case P::SideLeft:           return static_cast<uint64_t>(AV_CH_SIDE_LEFT);
        case P::SideRight:          return static_cast<uint64_t>(AV_CH_SIDE_RIGHT);
        case P::TopCenter:          return static_cast<uint64_t>(AV_CH_TOP_CENTER);
        case P::TopFrontLeft:       return static_cast<uint64_t>(AV_CH_TOP_FRONT_LEFT);
        case P::TopFrontCenter:     return static_cast<uint64_t>(AV_CH_TOP_FRONT_CENTER);
        case P::TopFrontRight:      return static_cast<uint64_t>(AV_CH_TOP_FRONT_RIGHT);
        case P::TopBackLeft:        return static_cast<uint64_t>(AV_CH_TOP_BACK_LEFT);
        case P::TopBackCenter:      return static_cast<uint64_t>(AV_CH_TOP_BACK_CENTER);
        case P::TopBackRight:       return static_cast<uint64_t>(AV_CH_TOP_BACK_RIGHT);
#ifdef AV_CH_LOW_FREQUENCY_2
        case P::LFE2:            return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY_2);
#endif
#ifdef AV_CH_TOP_SIDE_LEFT
        case P::TopSideLeft:     return static_cast<uint64_t>(AV_CH_TOP_SIDE_LEFT);
#endif
#ifdef AV_CH_TOP_SIDE_RIGHT
        case P::TopSideRight:    return static_cast<uint64_t>(AV_CH_TOP_SIDE_RIGHT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_CENTER
        case P::BottomFrontCenter: return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_CENTER);
#endif
#ifdef AV_CH_BOTTOM_FRONT_LEFT
        case P::BottomFrontLeft:   return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_LEFT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_RIGHT
        case P::BottomFrontRight:  return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_RIGHT);
#endif
        case P::UnknownPosition:
        default:
            return 0;
    }
}

std::optional<uint64_t> explicitMaskFromLayout(const AudioFormat& format)
{
    if(!format.hasChannelLayout())
        return {};

    const int channelCount = std::max(1, format.channelCount());
    const auto layout      = format.channelLayoutView();
    if(std::cmp_less(layout.size(), channelCount))
        return {};

    uint64_t mask{0};
    for(int i{0}; i < channelCount; ++i) {
        const uint64_t bit = channelBit(layout[static_cast<size_t>(i)]);
        if(bit == 0 || (mask & bit) != 0)
            return {};
        mask |= bit;
    }

    if(std::popcount(mask) != channelCount)
        return {};

    return mask;
}

#if OLD_CHANNEL_LAYOUT
uint64_t channelMaskForFormat(const AudioFormat& format)
{
    if(const auto explicitMask = explicitMaskFromLayout(format))
        return *explicitMask;

    const int channelCount   = std::max(1, format.channelCount());
    const int64_t defaultMsk = av_get_default_channel_layout(channelCount);
    if(defaultMsk > 0)
        return static_cast<uint64_t>(defaultMsk);

    return 0;
}
#else
bool channelLayoutForFormat(const AudioFormat& format, AVChannelLayout& layout)
{
    layout = {};

    const int channelCount = std::max(1, format.channelCount());
    if(const auto explicitMask = explicitMaskFromLayout(format)) {
        if(av_channel_layout_from_mask(&layout, *explicitMask) == 0
           && layout.nb_channels == channelCount) {
            return true;
        }
        av_channel_layout_uninit(&layout);
    }

    av_channel_layout_default(&layout, channelCount);
    return layout.nb_channels == channelCount;
}
#endif

AVSampleFormat avSampleFormatFor(const AudioFormat& format)
{
    switch(format.sampleFormat()) {
        case SampleFormat::U8:      return AV_SAMPLE_FMT_U8;
        case SampleFormat::S16:     return AV_SAMPLE_FMT_S16;
        case SampleFormat::S24In32:
        case SampleFormat::S32:     return AV_SAMPLE_FMT_S32;
        case SampleFormat::F32:     return AV_SAMPLE_FMT_FLT;
        case SampleFormat::F64:     return AV_SAMPLE_FMT_DBL;
        default:                    return AV_SAMPLE_FMT_NONE;
    }
}

// ---------------------------------------------------------------------------
// Swresample context — converts any PCM format to mono SoundTouch native fmt
// ---------------------------------------------------------------------------

struct SwrContextDeleter
{
    void operator()(SwrContext* ctx) const
    {
        if(ctx)
            swr_free(&ctx);
    }
};
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

// Creates a swresample context that converts `fmt` → mono, kSoundTouchAvFmt,
// same sample rate.  The mono mix-down is performed by swresample.
SwrContextPtr createMonoConverter(const AudioFormat& fmt)
{
    const AVSampleFormat inputFmt = avSampleFormatFor(fmt);
    if(inputFmt == AV_SAMPLE_FMT_NONE)
        return {};

    SwrContext* ctx = nullptr;

#if OLD_CHANNEL_LAYOUT
    const uint64_t inputMask = channelMaskForFormat(fmt);
    if(inputMask == 0)
        return {};

    ctx = swr_alloc_set_opts(nullptr,
                             AV_CH_LAYOUT_MONO, kSoundTouchAvFmt, fmt.sampleRate(),
                             static_cast<int64_t>(inputMask), inputFmt, fmt.sampleRate(),
                             0, nullptr);
    if(!ctx)
        return {};
#else
    AVChannelLayout inLayout{};
    AVChannelLayout outLayout{};

    if(!channelLayoutForFormat(fmt, inLayout)) {
        av_channel_layout_uninit(&inLayout);
        return {};
    }
    av_channel_layout_default(&outLayout, 1); // 1 channel = mono

    const int rc = swr_alloc_set_opts2(&ctx,
                                       &outLayout, kSoundTouchAvFmt, fmt.sampleRate(),
                                       &inLayout, inputFmt, fmt.sampleRate(),
                                       0, nullptr);
    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    if(rc < 0 || !ctx) {
        swr_free(&ctx);
        return {};
    }
#endif

    if(swr_init(ctx) < 0) {
        swr_free(&ctx);
        return {};
    }

    return SwrContextPtr{ctx};
}

// Converts one AudioBuffer to a vector of mono SoundTouch samples.
// Returns empty optional on converter failure, empty vector on zero-frame input.
std::optional<std::vector<SoundTouchSample>>
convertToMonoSamples(SwrContext* ctx, const AudioBuffer& buf)
{
    const int inFrames = buf.frameCount();
    if(inFrames <= 0)
        return std::vector<SoundTouchSample>{};

    const int maxOutFrames = swr_get_out_samples(ctx, inFrames);
    if(maxOutFrames <= 0)
        return {};

    std::vector<SoundTouchSample> out(static_cast<size_t>(maxOutFrames));

    const auto* inData = reinterpret_cast<const uint8_t*>(buf.data());
    auto*       outPtr = reinterpret_cast<uint8_t*>(out.data());

    const uint8_t* inPlanes[1]  = {inData};
    uint8_t*       outPlanes[1] = {outPtr};

    const int outFrames = swr_convert(ctx, outPlanes, maxOutFrames, inPlanes, inFrames);
    if(outFrames < 0)
        return {};

    out.resize(static_cast<size_t>(outFrames));
    return out;
}

// ---------------------------------------------------------------------------
// BPM aggregation from SoundTouch beat positions
// ---------------------------------------------------------------------------

constexpr float kMinBpm = 45.0f;
constexpr float kMaxBpm = 190.0f;

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
            return sumW > 0.0 ? static_cast<float>(sumWB / sumW) : fallbackBpm;
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

QString formatBpm(float bpm, int precision)
{
    switch(precision) {
        case 1:  return QString::number(static_cast<double>(bpm), 'f', 1);
        case 2:  return QString::number(static_cast<double>(bpm), 'f', 2);
        default: return QString::number(static_cast<int>(std::round(bpm)));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// BpmAnalyzerWorker
// ---------------------------------------------------------------------------

BpmAnalyzerWorker::BpmAnalyzerWorker(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

BpmResult BpmAnalyzerWorker::computeBpm(const Track& track,
                                        const QAtomicInt& cancelled) const
{
    // ---- Read settings ----
    FySettings settings;

    const auto method = static_cast<AggregationMethod>(
        settings.value(QLatin1String{SettingAggregationMethod}, DefaultAggregationMethod).toInt());

    const int sampleLength = std::clamp(
        settings.value(QLatin1String{SettingAnalysisSampleLength}, DefaultSampleLength).toInt(),
        1, 600);

    const bool skipExisting =
        settings.value(QLatin1String{SettingSkipExisting}, false).toBool();

    const int precision =
        settings.value(QLatin1String{SettingBpmPrecision}, DefaultBpmPrecision).toInt();

    // ---- Build result stub ----
    BpmResult result;
    result.track = track;

    const QStringList existingBpm = track.extraTag(u"BPM"_s);
    if(!existingBpm.isEmpty())
        result.storedBpm = existingBpm.first();

    if(skipExisting && !result.storedBpm.isEmpty()) {
        result.status = BpmResult::Status::Skipped;
        return result;
    }

    // ---- Open decoder ----
    const auto loaded = m_audioLoader->loadDecoderForTrack(
        track,
        AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);

    if(!loaded.decoder) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("No decoder available");
        return result;
    }

    const AudioFormat fmt = loaded.format.value_or(AudioFormat{});
    if(!fmt.isValid()) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Could not determine audio format");
        return result;
    }

    const int sampleRate  = fmt.sampleRate();
    const int inputChans  = fmt.channelCount();
    if(sampleRate <= 0 || inputChans <= 0) {
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Invalid audio format (zero rate or channels)");
        return result;
    }

    // ---- Create mono converter ----
    SwrContextPtr converter = createMonoConverter(fmt);
    if(!converter) {
        loaded.decoder->stop();
        result.status      = BpmResult::Status::Error;
        result.errorString = QObject::tr("Could not initialise audio format converter");
        return result;
    }

    // ---- Initialise BPMDetect ----
    // We feed mono samples, so numChannels = 1.
    soundtouch::BPMDetect detector(1, sampleRate);

    // ---- Decode and feed samples ----
    const qint64 maxFrames = static_cast<qint64>(sampleLength) * sampleRate;
    qint64       framesProcessed = 0;

    loaded.decoder->start();

    constexpr size_t ChunkBytes = 65536;
    while(!cancelled.loadRelaxed() && framesProcessed < maxFrames) {
        AudioBuffer buf = loaded.decoder->readBuffer(ChunkBytes);
        if(!buf.isValid() || buf.byteCount() == 0)
            break;

        auto samples = convertToMonoSamples(converter.get(), buf);
        if(!samples) {
            loaded.decoder->stop();
            result.status      = BpmResult::Status::Error;
            result.errorString = QObject::tr("Audio format conversion failed");
            return result;
        }

        if(!samples->empty()) {
            detector.inputSamples(samples->data(), static_cast<int>(samples->size()));
        }

        framesProcessed += buf.frameCount();
    }

    loaded.decoder->stop();

    if(cancelled.loadRelaxed()) {
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

    result.analyzedBpm = formatBpm(bpm, precision);
    result.status = result.storedBpm.isEmpty() ? BpmResult::Status::New
                                               : BpmResult::Status::Updated;
    return result;
}

} // namespace Fooyin::BpmAnalyzer
