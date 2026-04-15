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

#include <core/track.h>

#include <QString>

namespace Fooyin::BpmAnalyzer {

struct BpmResult
{
    enum class Status
    {
        New,      ///< No existing BPM tag; value freshly computed
        Updated,  ///< Track had an existing BPM tag; new value computed
        Skipped,  ///< Track had an existing BPM tag and skip-existing option is on
        Error     ///< Decoding or analysis failure
    };

    Track   track;
    QString analyzedBpm;  ///< The computed BPM as a formatted string (empty on error/skipped)
    QString storedBpm;    ///< Value from the existing BPM tag (may be empty)
    Status  status{Status::New};
    QString errorString;  ///< Populated when status == Error
};

} // namespace Fooyin::BpmAnalyzer
