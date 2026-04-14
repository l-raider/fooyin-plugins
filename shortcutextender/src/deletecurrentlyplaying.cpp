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

#include "deletecurrentlyplaying.h"

#include "deletedialog.h"
#include "deleteworker.h"
#include "shortcutextenderdefs.h"

#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include <QThread>

using namespace Qt::StringLiterals;

namespace Fooyin::ShortcutExtender {

DeleteCurrentlyPlaying::DeleteCurrentlyPlaying(ActionManager* actionManager, PlayerController* playerController,
                                               MusicLibrary* library, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_library{library}
    , m_settings{settings}
{
    auto* action = new QAction(tr("Delete currently playing File"), this);
    auto* cmd    = m_actionManager->registerAction(action, "ShortcutExtender.DeleteCurrentlyPlaying");
    cmd->setCategories({tr("Shortcut Extender")});
    Q_UNUSED(cmd)

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, action,
                     [this]() { updateActionState(); });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, action,
                     [this]() { updateActionState(); });

    updateActionState();

    QObject::connect(action, &QAction::triggered, this, &DeleteCurrentlyPlaying::onTriggered);
}

void DeleteCurrentlyPlaying::updateActionState()
{
    const Track track = m_playerController->currentTrack();
    // Disable if stopped, track invalid, or track is inside an archive (can't delete)
    const bool canDelete = m_playerController->playState() != Player::PlayState::Stopped
                        && track.isValid()
                        && !track.isInArchive();

    // The action is a child QObject; find it from children list
    for(auto* child : children()) {
        if(auto* action = qobject_cast<QAction*>(child)) {
            action->setEnabled(canDelete);
            break;
        }
    }
}

void DeleteCurrentlyPlaying::onTriggered()
{
    const Track playing = m_playerController->currentTrack();
    if(!playing.isValid() || playing.isInArchive()) {
        return;
    }

    const TrackList tracks{playing};

    const auto runDelete = [this, tracks]() {
        // Advance to the next track (or stop) before deleting so the audio engine
        // releases any file handle, preventing deletion failures and ensuring
        // consistent behaviour across all platforms.
        if(m_playerController->hasNextTrack()) {
            m_playerController->next();
        }
        else {
            m_playerController->stop();
        }

        const auto mode = static_cast<DeleteMode>(
            m_settings->fileValue(Settings::DeleteMode, static_cast<int>(DeleteMode::Trash)).toInt());

        auto* worker = new DeleteWorker(m_library, tracks, mode);
        auto* thread = new QThread(this);
        worker->moveToThread(thread);

        QObject::connect(worker, &DeleteWorker::deleteFinished, this, [](const TrackList& /*deletedTracks*/) {
            // Status bar notification: StatusEvent is a private fooyin header; deletion is
            // self-evident from the library view. Add status feedback here if the header
            // is made public in a future fooyin release.
        });
        QObject::connect(worker, &DeleteWorker::trashError, this, [](const QString& message) {
            QMessageBox::warning(Utils::getMainWindow(), tr("Move to Trash Failed"), message);
        });
        QObject::connect(worker, &Worker::finished, thread, &QThread::quit);
        QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
        QMetaObject::invokeMethod(worker, &DeleteWorker::deleteFiles);
    };

    const bool confirm = m_settings->fileValue(Settings::ConfirmDelete, true).toBool();
    if(confirm) {
        auto* dialog = new DeleteDialog(tracks, m_settings, Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dialog, &QDialog::accepted, dialog, [runDelete]() { runDelete(); });
        dialog->open();
    }
    else {
        runDelete();
    }
}

} // namespace Fooyin::ShortcutExtender
