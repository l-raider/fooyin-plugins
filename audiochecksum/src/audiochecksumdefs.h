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

#include <QString>

namespace Fooyin::AudioChecksum {

// Default tag field written to track metadata
constexpr auto DefaultTagFieldName = "AUDIOCHECKSUM_MD5";

// Settings keys
constexpr auto SettingTagField          = "AudioChecksum/TagField";
constexpr auto SettingSkipExisting      = "AudioChecksum/SkipExisting";
constexpr auto SettingConcurrencyAuto   = "AudioChecksum/ConcurrencyAuto";
constexpr auto SettingConcurrencyCount  = "AudioChecksum/ConcurrencyCount";
constexpr int  DefaultConcurrencyCount  = 1;

// Returns the currently configured tag field name, falling back to the default.
inline QString tagFieldName()
{
    FySettings s;
    return s.value(QLatin1String{SettingTagField},
                   QLatin1String{DefaultTagFieldName}).toString();
}

} // namespace Fooyin::AudioChecksum
