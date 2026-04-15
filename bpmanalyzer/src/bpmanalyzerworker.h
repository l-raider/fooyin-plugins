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

#pragma once

#include "bpmanalyzerresult.h"

#include <core/track.h>

#include <QAtomicInt>
#include <QObject>

#include <memory>

namespace Fooyin {
class AudioLoader;
} // namespace Fooyin

namespace Fooyin::BpmAnalyzer {

/*!
 * Pure computation helper that decodes a track to PCM, feeds the samples to
 * SoundTouch BPMDetect and returns an aggregated BPM value.
 *
 * computeBpm() is safe to call from multiple threads concurrently because
 * each invocation creates its own decoder and BPMDetect instance; there is
 * no shared mutable state other than the thread-safe AudioLoader.
 */
class BpmAnalyzerWorker : public QObject
{
    Q_OBJECT

public:
    explicit BpmAnalyzerWorker(std::shared_ptr<AudioLoader> audioLoader,
                               QObject* parent = nullptr);

    [[nodiscard]] BpmResult computeBpm(const Track& track,
                                       const QAtomicInt& cancelled) const;

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
};

} // namespace Fooyin::BpmAnalyzer
