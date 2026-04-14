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

#include "fileopspresetshortcuts.h"

#include "fileopsbridge.h"
#include "fileopsconfirmdialog.h"
#include "fileopsexecutor.h"
#include "shortcutextenderdefs.h"

#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/id.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include <QThread>

using namespace Qt::StringLiterals;

namespace Fooyin::ShortcutExtender {

namespace {
// Converts a preset name into a stable, printable ActionManager ID segment.
// Replaces any non-alphanumeric character with '_'.
QString sanitiseName(const QString& name)
{
    QString result = name;
    for(QChar& c : result) {
        if(!c.isLetterOrNumber()) {
            c = QLatin1Char('_');
        }
    }
    return result;
}
} // namespace

FileOpsPresetShortcuts::FileOpsPresetShortcuts(ActionManager*            actionManager,
                                               TrackSelectionController* trackSelection,
                                               PlayerController*         playerController,
                                               MusicLibrary*             library,
                                               QObject*                  parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_trackSelection{trackSelection}
    , m_playerController{playerController}
    , m_library{library}
{
    const auto presets    = getFileOpsPresets();
    const auto configMap  = loadPresetConfigs();

    for(const FileOpPreset& preset : presets) {
        const QString actionId = u"FileOps.Preset."_s + sanitiseName(preset.name);
        const QString label    = u"File operations preset: "_s + preset.name + u" ("_s + operationDisplayName(preset.op) + u")"_s;

        auto* action = new QAction(label, this);
        auto* cmd    = actionManager->registerAction(action, Id(actionId));
        cmd->setCategories({tr("Shortcut Extender")});
        Q_UNUSED(cmd)

        m_presetActions.push_back(action);

        // Capture config by value (falls back to defaults if not configured yet)
        const FileOpsPresetConfig config = configMap.value(preset.name);

        QObject::connect(action, &QAction::triggered, this,
                         [this, preset, config]() {
                             // Determine the track list
                             TrackList tracks;
                             if(config.trackSource == PresetTrackSource::Playing) {
                                 const Track t = m_playerController->currentTrack();
                                 if(t.isValid()) {
                                     tracks = {t};
                                 }
                             }
                             else {
                                 tracks = m_trackSelection->selectedTracks();
                             }

                             if(tracks.empty()) {
                                 return;
                             }

                             auto* thread   = new QThread();
                             auto* executor = new FileOpsExecutor(m_library, tracks);

                             // Quit the thread's event loop when the worker signals
                             // completion, then clean up once it has actually stopped.
                             QObject::connect(executor, &Worker::finished, thread, &QThread::quit);
                             QObject::connect(thread, &QThread::finished, executor, &QObject::deleteLater);
                             QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

                             if(config.execMode == PresetExecMode::Confirm) {
                                 // simulate() is called before moveToThread so that it runs
                                 // on the main thread — correct per Qt's ownership rules.
                                 const auto ops = executor->simulate(preset);

                                 if(ops.empty()) {
                                     // executor is still on the main thread; delete directly.
                                     delete executor;
                                     thread->deleteLater();
                                     QMessageBox::information(Utils::getMainWindow(),
                                                              tr("File Operations"),
                                                              tr("No operations to perform for preset \"%1\".")
                                                                  .arg(preset.name));
                                     return;
                                 }

                                 auto* dialog = new FileOpsConfirmDialog(preset.name, ops,
                                                                         Utils::getMainWindow());
                                 dialog->setAttribute(Qt::WA_DeleteOnClose);

                                 QObject::connect(dialog, &QDialog::accepted, thread,
                                                  [thread, executor, preset]() {
                                                      executor->moveToThread(thread);
                                                      QObject::connect(thread, &QThread::started, executor,
                                                                       [executor, preset]() {
                                                                           executor->execute(preset);
                                                                       });
                                                      thread->start();
                                                  });
                                 // executor is still on the main thread when rejected fires,
                                 // so delete directly rather than via deleteLater (which would
                                 // post to an event loop that was never started).
                                 QObject::connect(dialog, &QDialog::rejected, dialog,
                                                  [executor, thread]() {
                                                      delete executor;
                                                      thread->deleteLater();
                                                  });

                                 dialog->open();
                             }
                             else {
                                 // Direct mode: move to thread then execute immediately.
                                 executor->moveToThread(thread);
                                 QObject::connect(thread, &QThread::started, executor,
                                                  [executor, preset]() {
                                                      executor->execute(preset);
                                                  });
                                 thread->start();
                             }
                         });
    }

    // Keep action enabled state in sync with track availability
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged,
                     this, &FileOpsPresetShortcuts::updateActionStates);
    QObject::connect(m_playerController, &PlayerController::playStateChanged,
                     this, &FileOpsPresetShortcuts::updateActionStates);
    QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged,
                     this, &FileOpsPresetShortcuts::updateActionStates);

    updateActionStates();
}

void FileOpsPresetShortcuts::updateActionStates()
{
    const auto configMap = loadPresetConfigs();
    const auto presets   = getFileOpsPresets();

    for(int i = 0; i < static_cast<int>(m_presetActions.size()); ++i) {
        QAction* action = m_presetActions[static_cast<std::size_t>(i)];

        // Determine which source this action uses
        const QString presetName = i < static_cast<int>(presets.size()) ? presets[static_cast<std::size_t>(i)].name
                                                                         : QString{};
        const FileOpsPresetConfig cfg = configMap.value(presetName);

        bool canAct{false};
        if(cfg.trackSource == PresetTrackSource::Playing) {
            const Track t = m_playerController->currentTrack();
            canAct        = t.isValid() && !t.isInArchive()
                     && m_playerController->playState() != Player::PlayState::Stopped;
        }
        else {
            canAct = !m_trackSelection->selectedTracks().empty();
        }

        action->setEnabled(canAct);
    }
}

} // namespace Fooyin::ShortcutExtender
