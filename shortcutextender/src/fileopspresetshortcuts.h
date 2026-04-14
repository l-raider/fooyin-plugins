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

#include <QAction>
#include <QObject>

#include <vector>

namespace Fooyin {
class ActionManager;
class MusicLibrary;
class PlayerController;
class TrackSelectionController;

namespace ShortcutExtender {

// Reads all FileOps presets from the shared settings store at construction time
// and registers one QAction per preset in ActionManager so they appear in
// Settings → Shortcuts for user-configurable hotkeys.
//
// Registration is static (at startup); a fooyin restart is required to pick up
// preset additions/deletions/renames made in the FileOps plugin afterwards.
class FileOpsPresetShortcuts : public QObject
{
    Q_OBJECT

public:
    FileOpsPresetShortcuts(ActionManager* actionManager,
                           TrackSelectionController* trackSelection,
                           PlayerController* playerController,
                           MusicLibrary* library,
                           QObject* parent = nullptr);

private:
    void updateActionStates();

    ActionManager*            m_actionManager;
    TrackSelectionController* m_trackSelection;
    PlayerController*         m_playerController;
    MusicLibrary*             m_library;

    // One action per preset; owned as QAction children of this QObject.
    // Stored so we can update their enabled state.
    std::vector<QAction*> m_presetActions;
};

} // namespace ShortcutExtender
} // namespace Fooyin
