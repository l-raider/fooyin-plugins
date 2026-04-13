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

#include <core/track.h>

#include <QString>

namespace Fooyin::AudioChecksum {

struct ChecksumResult
{
    enum class Status
    {
        New,       // No stored checksum; computed for the first time
        Match,     // Computed matches stored checksum
        Mismatch,  // Computed does NOT match stored checksum
        Error      // Decoding or I/O failure
    };

    Track track;
    QString algorithm;       // e.g. "MD5"
    QString computedHash;    // hex string from decoded PCM
    QString storedHash;      // from AUDIOCHECKSUM tag (or FLAC STREAMINFO MD5)
    Status status{Status::New};
    QString errorString;     // populated only when status == Error
};

} // namespace Fooyin::AudioChecksum
