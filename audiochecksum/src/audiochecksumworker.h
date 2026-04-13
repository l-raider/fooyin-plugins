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
#include <QObject>

#include <memory>

namespace Fooyin {
class AudioLoader;
} // namespace Fooyin

namespace Fooyin::AudioChecksum {

/*!
 * Pure computation helper: decodes a track to PCM and hashes the audio data.
 *
 * computeChecksum() is safe to call from multiple threads concurrently as
 * long as the underlying AudioLoader is thread-safe for concurrent reads.
 * The cancelled flag is checked on every buffer iteration so that an in-
 * progress scan can be aborted promptly.
 */
class AudioChecksumWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader,
                                 QObject* parent = nullptr);

    [[nodiscard]] ChecksumResult computeChecksum(const Track& track,
                                                 const QAtomicInt& cancelled) const;

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
};

} // namespace Fooyin::AudioChecksum
