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

#include "audiochecksumdefs.h"
#include "audiochecksumworker.h"

#include <core/coresettings.h>

#include <QtConcurrent/QtConcurrent>
#include <QThread>

namespace Fooyin::AudioChecksum {

AudioChecksumScanner::AudioChecksumScanner(std::shared_ptr<AudioLoader> audioLoader,
                                           QObject* parent)
    : QObject{parent}
    , m_worker{std::make_unique<AudioChecksumWorker>(std::move(audioLoader))}
{
    QObject::connect(&m_watcher, &QFutureWatcher<ChecksumResult>::resultReadyAt,
                     this, &AudioChecksumScanner::onResultReadyAt);
    QObject::connect(&m_watcher, &QFutureWatcher<ChecksumResult>::finished,
                     this, &AudioChecksumScanner::onFinished);
}

AudioChecksumScanner::~AudioChecksumScanner()
{
    m_watcher.cancel();
    m_watcher.waitForFinished();
}

void AudioChecksumScanner::close()
{
    m_cancelled.storeRelaxed(1);
    m_watcher.cancel();
    // Don't waitForFinished() here — the destructor handles it safely.
    // Calling it while the watcher's finished signal is already queued on
    // the main thread causes a re-entrant / stale-callback crash.
}

void AudioChecksumScanner::scanTracks(const TrackList& tracks)
{
    m_cancelled.storeRelaxed(0);

    FySettings settings;
    const bool autoThreads = settings.value(QLatin1String{SettingConcurrencyAuto}, false).toBool();
    const int threadCount = autoThreads
        ? QThread::idealThreadCount()
        : std::max(1, settings.value(QLatin1String{SettingConcurrencyCount},
                                     DefaultConcurrencyCount).toInt());

    m_threadPool.setMaxThreadCount(threadCount);

    m_watcher.setFuture(QtConcurrent::mapped(
        &m_threadPool,
        tracks,
        [this](const Track& track) -> ChecksumResult {
            return m_worker->computeChecksum(track, m_cancelled);
        }
    ));
}

void AudioChecksumScanner::onResultReadyAt(int index)
{
    const ChecksumResult result = m_watcher.resultAt(index);
    emit scanningTrack(result.track.filepath());
    emit trackScanned(result);
}

void AudioChecksumScanner::onFinished()
{
    if(m_cancelled.loadRelaxed()) {
        emit scanFinished({});
        return;
    }
    emit scanFinished(m_watcher.future().results());
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumscanner.cpp"
