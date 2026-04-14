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

namespace Fooyin::ShortcutExtender {

enum class DeleteMode : int
{
    Trash     = 0,
    Permanent = 1,
};

namespace Settings {
inline constexpr auto ConfirmDelete = "ShortcutExtender/ConfirmDelete";
inline constexpr auto DeleteMode    = "ShortcutExtender/DeleteMode";
} // namespace Settings

} // namespace Fooyin::ShortcutExtender
