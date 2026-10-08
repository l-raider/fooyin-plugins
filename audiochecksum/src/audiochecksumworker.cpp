/*
 * Fooyin AudioChecksum Plugin
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

#include "audiochecksumworker.h"

#include "audiochecksumcancellation.h"
#include "audiochecksumdefs.h"
#include "flacstreaminfo.h"

#include <core/constants.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>
#include <utils/scopeguard.h>

#include <QCryptographicHash>
#include <QObject>

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

namespace {

//! CD audio sector size in frames (1/75 s at 44100 Hz), matching AccurateRip.
constexpr uint64_t FramesPerSector = 588;

bool matchesNativeWidth(SampleFormat format, int bitDepth)
{
    switch(format) {
        case SampleFormat::U8:
            return bitDepth == 8;
        case SampleFormat::S16:
            return bitDepth == 16;
        case SampleFormat::S24In32:
            return bitDepth == 24;
        case SampleFormat::S32:
            return bitDepth == 32;
        default:
            return false;
    }
}

bool addHashData(QCryptographicHash& hash, const AudioBuffer& buffer,
                 bool useFlacCanonicalMd5, const AudioFormat& s16Format)
{
    if(useFlacCanonicalMd5) {
        if(buffer.format().sampleFormat() == SampleFormat::S24In32) {
            // 24-bit FLAC is decoded to 32-bit samples left-aligned
            // (shifted << 8), so in LE memory each 4-byte word is
            // [0x00, LSB, MID, MSB]. The FLAC STREAMINFO MD5 is computed over
            // tightly-packed 3 bytes/sample LE data [LSB, MID, MSB], so we must
            // skip the low zero byte and copy only bytes [1,2,3] of each word.
            const auto* src = reinterpret_cast<const char*>(buffer.data());
            const int total = static_cast<int>(buffer.byteCount());
            const int count = total / 4;
            QByteArray packed(count * 3, Qt::Uninitialized);
            char* dst = packed.data();
            for(int i = 0; i < count; ++i) {
                std::memcpy(dst + i * 3, src + i * 4 + 1, 3);
            }
            hash.addData(packed);
            return true;
        }

        hash.addData(QByteArrayView{reinterpret_cast<const char*>(buffer.data()),
                                    static_cast<qsizetype>(buffer.byteCount())});
        return true;
    }

    if(!s16Format.isValid())
        return false;

    // Convert interleaved PCM to 16-bit signed via fooyin's Audio::convert.
    // The target format is a copy of the decoder format with only the sample
    // format changed, so channel order/count/layout are preserved and the
    // produced S16 stream is bit-identical to the previous swr-based path.
    const AudioBuffer converted = Audio::convert(buffer, s16Format);
    if(!converted.isValid())
        return false;

    hash.addData(QByteArrayView{reinterpret_cast<const char*>(converted.data()),
                                static_cast<qsizetype>(converted.byteCount())});
    return true;
}

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

} // namespace

AudioChecksumWorker::AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

ChecksumResult AudioChecksumWorker::computeChecksum(const Track& track,
                                                     CancellationToken& token) const
{
    ChecksumResult result;
    result.track = track;

    if(track.isRemote()) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Remote streams cannot be verified");
        return result;
    }

    const AudioChecksumSettings settings = AudioChecksumSettings::load();

    // Use fooyin's canonical editability check so CUE-bounded tracks (whose
    // metadata belongs to the shared container file) are never written.
    result.writable = track.isMetadataEditable(m_audioLoader->canWriteMetadata(track));

    // Retrieve any stored tag from the track
    const QStringList storedValues = track.extraTag(settings.tagField);
    if(!storedValues.isEmpty())
        result.storedHash = storedValues.first().toLower();

    // Decode and hash. ForConversion selects the offline source policy,
    // NoLooping disables all loop/repeat behaviour for the single pass and
    // VerifyIntegrity makes bitstream/CRC failures fatal (fooyin >= 0.13.0).
    const auto loaded = m_audioLoader->loadDecoderForTrack(
        track,
        AudioDecoder::NoLooping | AudioDecoder::ForConversion | AudioDecoder::VerifyIntegrity);

    if(!loaded.decoder || !loaded.format) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("No decoder available");
        return result;
    }

    const AudioFormat fmt = *loaded.format;
    if(!fmt.isValid()) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Could not determine audio format");
        return result;
    }

    const auto cueStartSector = cueSectorProperty(track, Constants::CueIndex01Sector);
    const auto cueEndSector   = cueSectorProperty(track, Constants::CueEndSector);

    if(cueStartSector && cueEndSector && *cueEndSector <= *cueStartSector) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Invalid CUE track boundaries");
        return result;
    }

    // The FLAC STREAMINFO MD5 covers the entire file, so it is only comparable
    // when this hash covers the whole file too. Bounded CUE/chapter segments
    // hash only their segment, and other sample widths (e.g. 20-bit FLAC
    // decoded as S32) can never match it, so both fall back to the computed
    // S16 hash and leave the STREAMINFO value out of the comparison.
    const bool wholeFile = !cueStartSector && !cueEndSector && track.offset() == 0
                        && !track.isBoundedSegment();
    const bool useFlacCanonicalMd5
        = isFlacTrack(track) && wholeFile && matchesNativeWidth(fmt.sampleFormat(), track.bitDepth());
    result.algorithm = useFlacCanonicalMd5 ? u"MD5 (FLAC)"_s : u"MD5 (S16)"_s;

    // For canonical FLAC: also extract the embedded STREAMINFO MD5 as a
    // reference. Overwrite storedHash only when no tag was set manually, so
    // user-provided tags take precedence over the encoder-embedded value.
    if(useFlacCanonicalMd5 && result.storedHash.isEmpty()) {
        const QString flacMd5 = readFlacStreamInfoMd5(track.filepath());
        if(!flacMd5.isEmpty())
            result.storedHash = flacMd5;
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
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Decoder cannot seek to the track segment");
            return result;
        }
        decoder->seek(track.offset());
    }

    uint64_t framesToSkip = cueStartSector.value_or(0) * FramesPerSector;
    std::optional<uint64_t> framesRemaining;

    if(cueStartSector && cueEndSector) {
        framesRemaining = (*cueEndSector - *cueStartSector) * FramesPerSector;
    }
    else if(track.isBoundedSegment() && track.duration() > 0 && !cueStartSector) {
        framesRemaining = framesForDuration(track.duration(), fmt.sampleRate());
    }

    // Target format for the non-canonical path: same decoder format but S16.
    // Copying the format preserves channel count/layout/rate so that
    // Audio::convert performs an identity channel map, matching the old
    // swr configuration (output layout == input layout).
    AudioFormat s16Format = fmt;
    s16Format.setSampleFormat(SampleFormat::S16);

    QCryptographicHash hash{QCryptographicHash::Md5};
    constexpr size_t ChunkBytes = 65536;

    while(!token.isCancelled()) {
        auto read = decoder->readAudio(ChunkBytes);

        if(read.status == AudioDecoder::ReadStatus::NeedMoreInput) {
            continue;
        }

        if(read.status == AudioDecoder::ReadStatus::Error) {
            result.status      = ChecksumResult::Status::Error;
            result.errorString = read.error.isEmpty() ? QObject::tr("Decoder error") : read.error;
            return result;
        }

        if(read.status == AudioDecoder::ReadStatus::EndOfStream) {
            if(framesToSkip > 0 || (framesRemaining && *framesRemaining > 0)) {
                result.status      = ChecksumResult::Status::Error;
                result.errorString = QObject::tr("Decoder ended before the track segment was complete");
                return result;
            }
            break;
        }

        AudioBuffer buffer = std::move(read.buffer);
        if(!buffer.isValid()) {
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Decoder returned invalid audio");
            return result;
        }

        if(framesToSkip > 0) {
            const uint64_t skipped
                = std::min<uint64_t>(framesToSkip, static_cast<uint64_t>(buffer.frameCount()));
            buffer = removeLeadingFrames(buffer, skipped);
            framesToSkip -= skipped;
            if(!buffer.isValid()) {
                continue;
            }
        }

        if(framesRemaining) {
            buffer = trimBuffer(buffer, *framesRemaining);
            *framesRemaining -= static_cast<uint64_t>(buffer.frameCount());
        }

        if(buffer.frameCount() > 0 && !addHashData(hash, buffer, useFlacCanonicalMd5, s16Format)) {
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Could not convert decoded audio to 16-bit PCM");
            return result;
        }

        if(framesRemaining && *framesRemaining == 0) {
            break;
        }
    }

    if(token.isCancelled()) {
        // Cancelled — return a partial/empty result rather than a wrong hash
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Cancelled");
        return result;
    }

    result.computedHash = QString::fromLatin1(hash.result().toHex());

    if(!result.storedHash.isEmpty()) {
        result.status = (result.computedHash == result.storedHash) ? ChecksumResult::Status::Match
                                                                   : ChecksumResult::Status::Mismatch;
    }
    // else: storedHash is empty — status stays at its default (New)

    return result;
}

} // namespace Fooyin::AudioChecksum
