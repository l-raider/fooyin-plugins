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
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>

#include <QCryptographicHash>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

AudioChecksumWorker::AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader,
                                         QObject* parent)
    : Worker{parent}
    , m_audioLoader{std::move(audioLoader)}
{ }

void AudioChecksumWorker::scanTracks(const TrackList& tracks)
{
    setState(Running);

    QList<ChecksumResult> results;
    results.reserve(static_cast<qsizetype>(tracks.size()));

    for(const Track& track : tracks) {
        if(!mayRun())
            break;

        emit scanningTrack(track.filepath());

        const ChecksumResult result = computeChecksum(track);
        emit trackScanned(result);
        results.append(result);
    }

    setState(Idle);
    emit scanFinished(results);
}

ChecksumResult AudioChecksumWorker::computeChecksum(const Track& track) const
{
    ChecksumResult result;
    result.track     = track;
    result.algorithm = u"MD5"_s;

    // Retrieve any stored tag from the track
    const QStringList storedValues = track.extraTag(tagFieldName());
    if(!storedValues.isEmpty())
        result.storedHash = storedValues.first().toLower();

    // For FLAC: also extract the embedded STREAMINFO MD5 as a reference.
    // Overwrite storedHash only when no tag was set manually, so user-provided
    // tags take precedence over the encoder-embedded value.
    const QString codec = track.codec().toLower();
    if(codec == "flac"_L1 || track.filepath().endsWith(".flac"_L1, Qt::CaseInsensitive)) {
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

    QCryptographicHash hash{QCryptographicHash::Md5};

    constexpr size_t ChunkBytes = 65536;
    while(mayRun()) {
        AudioBuffer buffer = loaded.decoder->readBuffer(ChunkBytes);
        if(!buffer.isValid() || buffer.byteCount() == 0)
            break;

        hash.addData(QByteArrayView{reinterpret_cast<const char*>(buffer.data()),
                                    static_cast<qsizetype>(buffer.byteCount())});
    }

    loaded.decoder->stop();

    if(!mayRun()) {
        // Cancelled — return a partial/empty result rather than a wrong hash
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Cancelled");
        return result;
    }

    result.computedHash = QString::fromLatin1(hash.result().toHex());

    if(result.storedHash.isEmpty()) {
        result.status = ChecksumResult::Status::New;
    }
    else if(result.computedHash == result.storedHash) {
        result.status = ChecksumResult::Status::Match;
    }
    else {
        result.status = ChecksumResult::Status::Mismatch;
    }

    return result;
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumworker.cpp"
