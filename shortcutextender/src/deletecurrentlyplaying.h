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

#include <QObject>

namespace Fooyin {
class ActionManager;
class MusicLibrary;
class PlayerController;
class SettingsManager;

namespace ShortcutExtender {

/*!
 * Registers and handles the "Delete Currently Playing" action.
 *
 * Owns the QAction lifetime, updates its enabled state in response to
 * playback changes, and runs the delete worker on a dedicated thread.
 *
 * To add a new shortcut, create an analogous class alongside this one
 * and instantiate it in ShortcutExtenderPlugin::initialise(GuiPluginContext).
 */
class DeleteCurrentlyPlaying : public QObject
{
    Q_OBJECT

public:
    DeleteCurrentlyPlaying(ActionManager* actionManager, PlayerController* playerController,
                           MusicLibrary* library, SettingsManager* settings, QObject* parent = nullptr);

private:
    void onTriggered();
    void updateActionState();

    ActionManager*    m_actionManager;
    PlayerController* m_playerController;
    MusicLibrary*     m_library;
    SettingsManager*  m_settings;
};

} // namespace ShortcutExtender
} // namespace Fooyin
