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

#include "fileopsbridge.h"

#include <core/coresettings.h>

#include <QByteArray>
#include <QDataStream>
#include <QMap>
#include <QString>

namespace Fooyin::ShortcutExtender {

enum class DeleteMode : int
{
    Trash     = 0,
    Permanent = 1,
};

// Which tracks a preset shortcut should operate on
enum class PresetTrackSource : int
{
    Selected = 0, // Tracks currently selected in the UI
    Playing  = 1, // Currently playing track
};

// What a preset shortcut should do when triggered
enum class PresetExecMode : int
{
    Confirm = 0, // Show a preview/confirm dialog before executing
    Direct  = 1, // Execute immediately without confirmation
};

struct FileOpsPresetConfig
{
    PresetTrackSource trackSource{PresetTrackSource::Selected};
    PresetExecMode    execMode{PresetExecMode::Confirm};
};

namespace Settings {
inline constexpr auto ConfirmDelete       = "ShortcutExtender/ConfirmDelete";
inline constexpr auto DeleteMode          = "ShortcutExtender/DeleteMode";
inline constexpr auto FileOpsPresetConfig = "ShortcutExtender/FileOpsPresetConfigs";
} // namespace Settings

// Maps preset name -> per-preset shortcut config
using PresetConfigMap = QMap<QString, FileOpsPresetConfig>;

inline PresetConfigMap loadPresetConfigs()
{
    const FySettings settings;
    auto byteArray = settings.value(QLatin1StringView{Settings::FileOpsPresetConfig}).toByteArray();
    if(byteArray.isEmpty()) {
        return {};
    }
    byteArray = qUncompress(byteArray);
    if(byteArray.isEmpty()) {
        return {};
    }

    QDataStream stream(&byteArray, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    PresetConfigMap result;
    qint32 count{0};
    stream >> count;
    while(count-- > 0) {
        QString key;
        int src{0};
        int mode{0};
        stream >> key >> src >> mode;
        result[key] = FileOpsPresetConfig{static_cast<PresetTrackSource>(src), static_cast<PresetExecMode>(mode)};
    }
    return result;
}

inline void savePresetConfigs(const PresetConfigMap& configs)
{
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<qint32>(configs.size());
    for(auto it = configs.cbegin(); it != configs.cend(); ++it) {
        stream << it.key()
               << static_cast<int>(it.value().trackSource)
               << static_cast<int>(it.value().execMode);
    }
    byteArray = qCompress(byteArray, 9);
    FySettings s;
    s.setValue(QLatin1StringView{Settings::FileOpsPresetConfig}, byteArray);
}

} // namespace Fooyin::ShortcutExtender
