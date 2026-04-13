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

#include <QString>

namespace Fooyin::AudioChecksum {

/*!
 * Reads the 16-byte MD5 stored in a FLAC file's STREAMINFO metadata block.
 *
 * The MD5 is computed by the FLAC encoder from the raw integer PCM samples
 * before compression. It is embedded in the STREAMINFO block and can be
 * read without decoding the audio.
 *
 * Returns a 32-character lowercase hex string on success, or an empty
 * QString if the file is not a valid FLAC file or the MD5 is all-zeroes
 * (i.e. the encoder did not set it).
 */
QString readFlacStreamInfoMd5(const QString& filePath);

} // namespace Fooyin::AudioChecksum
