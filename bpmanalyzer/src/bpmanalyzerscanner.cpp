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

#include "bpmanalyzerscanner.h"

#include "bpmanalyzerdefs.h"
#include "bpmanalyzerworker.h"

#include <QtConcurrent/QtConcurrent>
#include <QThread>

#include <algorithm>

namespace Fooyin::BpmAnalyzer {

BpmAnalyzerScanner::BpmAnalyzerScanner(std::shared_ptr<AudioLoader> audioLoader,
                                       QObject* parent)
    : QObject{parent}
    , m_worker{std::make_unique<BpmAnalyzerWorker>(std::move(audioLoader))}
{
    QObject::connect(&m_watcher, &QFutureWatcher<BpmResult>::resultReadyAt,
                     this, &BpmAnalyzerScanner::onResultReadyAt);
    QObject::connect(&m_watcher, &QFutureWatcher<BpmResult>::finished,
                     this, &BpmAnalyzerScanner::onFinished);
}

BpmAnalyzerScanner::~BpmAnalyzerScanner()
{
    m_token.cancel();
    m_watcher.cancel();
    m_watcher.waitForFinished();
}

void BpmAnalyzerScanner::close()
{
    m_token.cancel();
    m_watcher.cancel();
    // Don't waitForFinished() here — the destructor handles it safely.
    // Calling it while the watcher's finished signal is already queued on
    // the main thread causes a re-entrant / stale-callback crash.
}

void BpmAnalyzerScanner::scanTracks(const TrackList& tracks)
{
    m_token.reset();

    const BpmAnalyzerSettings settings = BpmAnalyzerSettings::load();
    const int threadCount = settings.concurrencyAuto
        ? std::max(1, QThread::idealThreadCount())
        : std::max(1, settings.concurrencyCount);

    m_threadPool.setMaxThreadCount(threadCount);

    m_watcher.setFuture(QtConcurrent::mapped(
        &m_threadPool,
        tracks,
        [this](const Track& track) -> BpmResult {
            return m_worker->computeBpm(track, m_token);
        }
    ));
}

void BpmAnalyzerScanner::onResultReadyAt(int index)
{
    const BpmResult result = m_watcher.resultAt(index);
    emit scanningTrack(result.track.filepath());
    emit trackScanned(result);
}

void BpmAnalyzerScanner::onFinished()
{
    emit scanFinished();
}

} // namespace Fooyin::BpmAnalyzer

#include "moc_bpmanalyzerscanner.cpp"
