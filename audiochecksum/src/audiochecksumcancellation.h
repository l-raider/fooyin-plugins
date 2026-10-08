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

#include <core/engine/audioinput.h>

#include <QAtomicInt>

#include <algorithm>
#include <mutex>
#include <vector>

namespace Fooyin::AudioChecksum {

/*!
 * Cancellation token shared between a scanner and its workers.
 *
 * The flag can be polled cheaply between reads, while cancel() also asks any
 * registered decoder to abort an in-progress blocking read via
 * AudioDecoder::requestAbort(). All methods are thread-safe.
 */
class CancellationToken
{
public:
    [[nodiscard]] bool isCancelled() const
    {
        return m_cancelled.loadRelaxed() != 0;
    }

    void reset()
    {
        m_cancelled.storeRelaxed(0);
    }

    void cancel()
    {
        m_cancelled.storeRelaxed(1);

        // Hold the mutex while calling out: unregisterDecoder() takes the same
        // mutex, so this guarantees a worker cannot destroy its decoder while
        // requestAbort() is still using it. requestAbort() is documented as
        // thread-safe, prompt, and non-tearing-down, so it cannot call back
        // into this token or block on decoder state.
        const std::scoped_lock lock{m_mutex};
        for(AudioDecoder* decoder : m_decoders) {
            decoder->requestAbort();
        }
    }

    void registerDecoder(AudioDecoder* decoder)
    {
        const std::scoped_lock lock{m_mutex};
        if(std::ranges::find(m_decoders, decoder) == m_decoders.end()) {
            m_decoders.push_back(decoder);
        }
        // A cancel() may have raced with registration; make sure this decoder
        // is aborted too.
        if(isCancelled()) {
            decoder->requestAbort();
        }
    }

    void unregisterDecoder(AudioDecoder* decoder)
    {
        const std::scoped_lock lock{m_mutex};
        std::erase(m_decoders, decoder);
    }

private:
    QAtomicInt m_cancelled{0};
    std::mutex m_mutex;
    std::vector<AudioDecoder*> m_decoders;
};

} // namespace Fooyin::AudioChecksum
