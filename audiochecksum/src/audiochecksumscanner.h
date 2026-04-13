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

#include <QAtomicInt>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>

#include <memory>

namespace Fooyin {
class AudioLoader;
} // namespace Fooyin

namespace Fooyin::AudioChecksum {

class AudioChecksumWorker;

/*!
 * Main-thread facade for parallel audio checksum scanning.
 *
 * Uses QtConcurrent::mapped with a private QThreadPool so that the thread
 * count is configurable at runtime from the settings.  Signals are emitted
 * from the main thread via QFutureWatcher.
 */
class AudioChecksumScanner : public QObject
{
    Q_OBJECT

public:
    explicit AudioChecksumScanner(std::shared_ptr<AudioLoader> audioLoader,
                                  QObject* parent = nullptr);
    ~AudioChecksumScanner() override;

    void close();

    void scanTracks(const TrackList& tracks);

signals:
    void scanningTrack(const QString& filepath);
    void trackScanned(const Fooyin::AudioChecksum::ChecksumResult& result);
    void scanFinished(const QList<Fooyin::AudioChecksum::ChecksumResult>& results);

private:
    void onResultReadyAt(int index);
    void onFinished();

    std::unique_ptr<AudioChecksumWorker> m_worker;
    QFutureWatcher<ChecksumResult>       m_watcher;
    QThreadPool                          m_threadPool;
    QAtomicInt                           m_cancelled{0};
};

} // namespace Fooyin::AudioChecksum
