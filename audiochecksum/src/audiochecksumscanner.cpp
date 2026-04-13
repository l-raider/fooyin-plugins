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

#include "audiochecksumscanner.h"

#include "audiochecksumworker.h"

namespace Fooyin::AudioChecksum {

AudioChecksumScanner::AudioChecksumScanner(std::shared_ptr<AudioLoader> audioLoader,
                                           QObject* parent)
    : QObject{parent}
    , m_worker{std::make_unique<AudioChecksumWorker>(std::move(audioLoader))}
{
    m_worker->moveToThread(&m_scanThread);
    m_scanThread.start();

    QObject::connect(m_worker.get(), &AudioChecksumWorker::scanningTrack,
                     this, &AudioChecksumScanner::scanningTrack);
    QObject::connect(m_worker.get(), &AudioChecksumWorker::trackScanned,
                     this, &AudioChecksumScanner::trackScanned);
    QObject::connect(m_worker.get(), &AudioChecksumWorker::scanFinished,
                     this, &AudioChecksumScanner::scanFinished);
}

AudioChecksumScanner::~AudioChecksumScanner()
{
    m_scanThread.quit();
    m_scanThread.wait();
}

void AudioChecksumScanner::close()
{
    m_worker->closeThread();
}

void AudioChecksumScanner::scanTracks(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        m_worker->scanTracks(tracks);
    });
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumscanner.cpp"
