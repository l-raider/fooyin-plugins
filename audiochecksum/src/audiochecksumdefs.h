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

#include <core/coresettings.h>
#include <core/track.h>

#include <QString>

namespace Fooyin::AudioChecksum {

// Default tag field written to track metadata
constexpr auto DefaultTagFieldName = "AUDIOCHECKSUM_MD5";

// Settings keys
constexpr auto SettingTagField          = "AudioChecksum/TagField";
constexpr auto SettingConcurrencyAuto   = "AudioChecksum/ConcurrencyAuto";
constexpr auto SettingConcurrencyCount  = "AudioChecksum/ConcurrencyCount";
constexpr int  DefaultConcurrencyCount  = 1;

//! Single source of truth for the plugin's persisted settings.
struct AudioChecksumSettings
{
    QString tagField{QLatin1String{DefaultTagFieldName}};
    bool concurrencyAuto{false};
    int concurrencyCount{DefaultConcurrencyCount};

    static AudioChecksumSettings load()
    {
        FySettings settings;
        AudioChecksumSettings result;
        result.tagField = settings.value(QLatin1String{SettingTagField},
                                         QLatin1String{DefaultTagFieldName}).toString();
        result.concurrencyAuto = settings.value(QLatin1String{SettingConcurrencyAuto}, false).toBool();
        result.concurrencyCount = settings.value(QLatin1String{SettingConcurrencyCount},
                                                 DefaultConcurrencyCount).toInt();
        return result;
    }

    void save() const
    {
        FySettings settings;
        settings.setValue(QLatin1String{SettingTagField}, tagField);
        settings.setValue(QLatin1String{SettingConcurrencyAuto}, concurrencyAuto);
        settings.setValue(QLatin1String{SettingConcurrencyCount}, concurrencyCount);
    }
};

//! True for tracks that carry an authoritative FLAC STREAMINFO MD5.
inline bool isFlacTrack(const Track& track)
{
    const QString codec = track.codec().toLower();
    return codec == QLatin1StringView{"flac"}
        || track.filepath().endsWith(QLatin1StringView{".flac"}, Qt::CaseInsensitive);
}

} // namespace Fooyin::AudioChecksum
