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

#include "flacstreaminfo.h"

#include <QFile>

namespace Fooyin::AudioChecksum {

/*
 * FLAC file structure (relevant portion):
 *
 *  Bytes 0-3:   "fLaC" magic
 *  Bytes 4+:    One or more metadata blocks, each with a 4-byte header:
 *    Bit 31:       Last-metadata-block flag
 *    Bits 30-24:   Block type (0 = STREAMINFO)
 *    Bits 23-0:    Length of block body in bytes
 *
 * STREAMINFO body (34 bytes total):
 *    Bytes 0-1:   Min block size (16-bit)
 *    Bytes 2-3:   Max block size (16-bit)
 *    Bytes 4-6:   Min frame size (24-bit)
 *    Bytes 7-9:   Max frame size (24-bit)
 *    Bits 80-100: Sample rate (20-bit)
 *    Bits 100-102:Channel count minus 1 (3-bit)
 *    Bits 103-107:Bits per sample minus 1 (5-bit)
 *    Bits 108-143:Total samples (36-bit)
 *    Bytes 18-33: MD5 of uncompressed PCM (16 bytes)
 */

QString readFlacStreamInfoMd5(const QString& filePath)
{
    QFile file{filePath};
    if(!file.open(QIODevice::ReadOnly))
        return {};

    // Check "fLaC" magic
    const QByteArray magic = file.read(4);
    if(magic != "fLaC")
        return {};

    // Iterate metadata blocks looking for STREAMINFO (type 0)
    while(!file.atEnd()) {
        const QByteArray header = file.read(4);
        if(header.size() < 4)
            return {};

        const auto h = static_cast<quint32>(
            (static_cast<quint8>(header[0]) << 24)
            | (static_cast<quint8>(header[1]) << 16)
            | (static_cast<quint8>(header[2]) << 8)
            | static_cast<quint8>(header[3]));

        const int blockType   = static_cast<int>((h >> 24) & 0x7F);
        const qint64 bodySize = static_cast<qint64>(h & 0x00FFFFFF);

        if(blockType == 0) {
            // STREAMINFO: must be exactly 34 bytes
            if(bodySize < 34)
                return {};

            const QByteArray body = file.read(bodySize);
            if(body.size() < 34)
                return {};

            // MD5 is at offset 18 in the STREAMINFO body
            const QByteArray md5Bytes = body.mid(18, 16);

            // All-zeroes means the encoder did not compute an MD5
            const bool allZero = std::all_of(md5Bytes.cbegin(), md5Bytes.cend(),
                                             [](char c) { return c == '\0'; });
            if(allZero)
                return {};

            return QString::fromLatin1(md5Bytes.toHex());
        }

        // Skip this block's body
        if(!file.seek(file.pos() + bodySize))
            return {};

        // If this was the last metadata block, stop searching
        const bool isLast = (h >> 31) & 1;
        if(isLast)
            break;
    }

    return {};
}

} // namespace Fooyin::AudioChecksum
