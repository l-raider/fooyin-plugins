/*
 * Fooyin
 * Copyright © 2026
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

// Mirrors the FileOpPreset struct and preset persistence from the fileops plugin
// without linking against it. Deserialization logic mirrors fileopssettings.cpp.

#include <core/coresettings.h>

#include <QByteArray>
#include <QDataStream>
#include <QString>

#include <vector>

namespace Fooyin::ShortcutExtender {

// Mirrors Fooyin::FileOps::Operation
enum class FileOpsOperation : uint8_t
{
    Copy   = 0,
    Move   = 1,
    Rename = 2,
    Create = 3,
    Remove = 4,
    Delete = 5,
};

// Mirrors Fooyin::FileOps::FileOpPreset
struct FileOpPreset
{
    FileOpsOperation op{FileOpsOperation::Copy};
    QString name;
    QString dest;
    QString filename;
    bool overwrite{false};
    bool wholeDir{false};
    bool removeEmpty{false};

    friend QDataStream& operator<<(QDataStream& stream, const FileOpPreset& preset)
    {
        stream << preset.op;
        stream << preset.name;
        stream << preset.dest;
        stream << preset.filename;
        stream << preset.overwrite;
        stream << preset.wholeDir;
        stream << preset.removeEmpty;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FileOpPreset& preset)
    {
        stream >> preset.op;
        stream >> preset.name;
        stream >> preset.dest;
        stream >> preset.filename;
        stream >> preset.overwrite;
        stream >> preset.wholeDir;
        stream >> preset.removeEmpty;
        return stream;
    }
};

// Deserializes all FileOps presets from fooyin's shared settings.
// Mirrors FileOps::getPresets() in fileopssettings.cpp.
inline std::vector<FileOpPreset> getFileOpsPresets()
{
    std::vector<FileOpPreset> presets;

    const FySettings settings;
    auto byteArray = settings.value(QStringLiteral("FileOps/Presets")).toByteArray();

    if(!byteArray.isEmpty()) {
        byteArray = qUncompress(byteArray);

        QDataStream stream(&byteArray, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        qint32 size{0};
        stream >> size;

        while(size > 0) {
            --size;
            FileOpPreset preset;
            stream >> preset;
            presets.push_back(preset);
        }
    }

    return presets;
}

inline QString operationDisplayName(FileOpsOperation op)
{
    switch(op) {
        case FileOpsOperation::Copy:
            return QStringLiteral("Copy");
        case FileOpsOperation::Move:
            return QStringLiteral("Move");
        case FileOpsOperation::Rename:
            return QStringLiteral("Rename");
        case FileOpsOperation::Create:
            return QStringLiteral("Create");
        case FileOpsOperation::Remove:
            return QStringLiteral("Remove");
        case FileOpsOperation::Delete:
            return QStringLiteral("Delete");
    }
    return {};
}

} // namespace Fooyin::ShortcutExtender
