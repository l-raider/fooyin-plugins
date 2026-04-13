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

#pragma once

#include "audiochecksumresult.h"

#include <core/track.h>
#include <utils/worker.h>

#include <QObject>

#include <memory>

namespace Fooyin {
class AudioLoader;
} // namespace Fooyin

namespace Fooyin::AudioChecksum {

/*!
 * Background worker that decodes each track to PCM and feeds the audio data
 * into QCryptographicHash to produce a content checksum.
 *
 * Must be moved to a QThread before calling scanTracks().
 */
class AudioChecksumWorker : public Worker
{
    Q_OBJECT

public:
    explicit AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader,
                                 QObject* parent = nullptr);

    void scanTracks(const TrackList& tracks);

signals:
    void scanningTrack(const QString& filepath);
    void trackScanned(const Fooyin::AudioChecksum::ChecksumResult& result);
    void scanFinished(const QList<Fooyin::AudioChecksum::ChecksumResult>& results);

private:
    [[nodiscard]] ChecksumResult computeChecksum(const Track& track) const;

    std::shared_ptr<AudioLoader> m_audioLoader;
};

} // namespace Fooyin::AudioChecksum
