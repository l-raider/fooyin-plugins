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

#include <QDir>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

// Returns the writable XDG data home directory, honouring the Flatpak sandbox.
//
// Inside a Flatpak sandbox QStandardPaths::GenericDataLocation resolves to the
// app-private container (~/.var/app/<id>/data) rather than the real
// ~/.local/share.  Files trashed there are invisible to the host desktop and
// cannot be restored through any file manager.  We detect the sandbox via the
// FLATPAK_ID environment variable and, since fooyin's manifest grants the
// 'home' filesystem permission, use $HOME/.local/share directly so that
// trashed files end up in the host user's trash.
[[nodiscard]] inline QString xdgDataHome()
{
    if(qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return QDir::homePath() + u"/.local/share"_s;
    }
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}
