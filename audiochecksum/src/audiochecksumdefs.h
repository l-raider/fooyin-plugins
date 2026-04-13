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

namespace Fooyin::AudioChecksum {

// Tag field written to track metadata
constexpr auto TagFieldName = "AUDIOCHECKSUM_MD5";

// Settings keys (for future settings page)
constexpr auto SettingTagField     = "AudioChecksum/TagField";
constexpr auto SettingSkipExisting = "AudioChecksum/SkipExisting";

// Context menu ID
constexpr auto MenuAudioChecksum = "AudioChecksum.Menu";

} // namespace Fooyin::AudioChecksum
