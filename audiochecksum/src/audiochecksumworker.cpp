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

#include "audiochecksumdefs.h"
#include "flacstreaminfo.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioconverter.h>
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <cstring>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

namespace {

bool isFlacTrack(const Track& track)
{
    const QString codec = track.codec().toLower();
    return codec == u"flac"_s
        || track.filepath().endsWith(u".flac"_s, Qt::CaseInsensitive);
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

} // namespace

AudioChecksumWorker::AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader,
                                         QObject* parent)
    : QObject{parent}
    , m_audioLoader{std::move(audioLoader)}
{ }

ChecksumResult AudioChecksumWorker::computeChecksum(const Track& track,
                                                     const QAtomicInt& cancelled) const
{
    ChecksumResult result;
    result.track = track;

    const bool useFlacCanonicalMd5 = isFlacTrack(track);
    result.algorithm = useFlacCanonicalMd5 ? u"MD5 (FLAC)"_s
                                           : u"MD5 (S16)"_s;

    // Retrieve any stored tag from the track
    const QStringList storedValues = track.extraTag(tagFieldName());
    if(!storedValues.isEmpty())
        result.storedHash = storedValues.first().toLower();

    // For FLAC: also extract the embedded STREAMINFO MD5 as a reference.
    // Overwrite storedHash only when no tag was set manually, so user-provided
    // tags take precedence over the encoder-embedded value.
    if(useFlacCanonicalMd5) {
        const QString flacMd5 = readFlacStreamInfoMd5(track.filepath());
        if(!flacMd5.isEmpty() && result.storedHash.isEmpty())
            result.storedHash = flacMd5;
    }

    // Decode and hash
    const auto loaded = m_audioLoader->loadDecoderForTrack(
        track,
        AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);

    if(!loaded.decoder) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("No decoder available");
        return result;
    }

    const AudioFormat fmt = loaded.format.value_or(AudioFormat{});
    if(!fmt.isValid()) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Could not determine audio format");
        return result;
    }

    loaded.decoder->start();

    // Target format for the non-FLAC path: same decoder format but S16.
    // Copying the format preserves channel count/layout/rate so that
    // Audio::convert performs an identity channel map, matching the old
    // swr configuration (output layout == input layout).
    AudioFormat s16Format = fmt;
    s16Format.setSampleFormat(SampleFormat::S16);

    QCryptographicHash hash{QCryptographicHash::Md5};
    constexpr size_t ChunkBytes = 65536;
    while(!cancelled.loadRelaxed()) {
        AudioBuffer buffer = loaded.decoder->readBuffer(ChunkBytes);
        if(!buffer.isValid() || buffer.byteCount() == 0)
            break;

        if(!addHashData(hash, buffer, useFlacCanonicalMd5, s16Format)) {
            loaded.decoder->stop();
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Could not convert decoded audio to 16-bit PCM");
            return result;
        }
    }

    loaded.decoder->stop();

    if(cancelled.loadRelaxed()) {
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

#include "moc_audiochecksumworker.cpp"
