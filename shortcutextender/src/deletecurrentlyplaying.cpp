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
#include <QDir>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QStandardPaths>
#include <QThread>

using namespace Qt::StringLiterals;

namespace {
QString xdgDataHome()
{
    if(qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return QDir::homePath() + u"/.local/share"_s;
    }
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}
} // namespace

namespace Fooyin::ShortcutExtender {

DeleteCurrentlyPlaying::DeleteCurrentlyPlaying(ActionManager* actionManager, PlayerController* playerController,
                                               MusicLibrary* library, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_playerController{playerController}
    , m_library{library}
    , m_settings{settings}
{
    m_action = new QAction(tr("Delete currently playing File"), this);
    auto* cmd = actionManager->registerAction(m_action, "ShortcutExtender.DeleteCurrentlyPlaying");
    cmd->setCategories({tr("Shortcut Extender")});
    Q_UNUSED(cmd)

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_action,
                     [this]() { updateActionState(); });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_action,
                     [this]() { updateActionState(); });

    updateActionState();

    QObject::connect(m_action, &QAction::triggered, this, &DeleteCurrentlyPlaying::onTriggered);
}

void DeleteCurrentlyPlaying::updateActionState()
{
    const Track track = m_playerController->currentTrack();
    // Disable if stopped, track invalid, or track is inside an archive (can't delete)
    const bool canDelete = m_playerController->playState() != Player::PlayState::Stopped
                        && track.isValid()
                        && !track.isInArchive();

    m_action->setEnabled(canDelete);
}

void DeleteCurrentlyPlaying::onTriggered()
{
    const Track playing = m_playerController->currentTrack();
    if(!playing.isValid() || playing.isInArchive()) {
        return;
    }

    const TrackList tracks{playing};

    const auto mode = static_cast<DeleteMode>(
        m_settings->fileValue(Settings::DeleteMode, static_cast<int>(DeleteMode::Trash)).toInt());

    const auto runDelete = [this, tracks, mode]() {
        const QFileInfo fileInfo{tracks.front().filepath()};
        const QString dirPath = fileInfo.dir().path();

        if(!fileInfo.dir().exists() || !QFileInfo(dirPath).isWritable()) {
            QMessageBox::warning(Utils::getMainWindow(), tr("Delete Failed"),
                                 tr("No write permission to:\n%1").arg(dirPath));
            return;
        }

        // When trashing, verify the trash directories are writable before
        // advancing playback — the source dir being writable is not enough.
        // Only check directories that already exist: on a fresh user account
        // the Trash sub-directories don't exist yet and QFileInfo::isWritable()
        // returns false for non-existent paths, giving a false permission error.
        // The worker creates them via mkpath on first use and reports any real
        // failure there.
        if(mode == DeleteMode::Trash) {
            const QString dataHome   = xdgDataHome();
            const QString trashFiles = dataHome + u"/Trash/files"_s;
            const QString trashInfo  = dataHome + u"/Trash/info"_s;

            const QFileInfo trashFilesInfo{trashFiles};
            const QFileInfo trashInfoInfo{trashInfo};
            if((trashFilesInfo.exists() && !trashFilesInfo.isWritable())
               || (trashInfoInfo.exists() && !trashInfoInfo.isWritable())) {
                QMessageBox::warning(Utils::getMainWindow(), tr("Move to Trash Failed"),
                                     tr("No write permission to the trash directory:\n%1\n\n"
                                        "To delete files permanently instead, open Settings and "
                                        "change \"Delete mode\" to \"Delete permanently\" under "
                                        "Shortcut Extender.").arg(trashInfo));
                return;
            }
        }

        // Advance to the next track (or stop) before deleting so the audio engine
        // releases any file handle, preventing deletion failures and ensuring
        // consistent behaviour across all platforms.
        if(m_playerController->hasNextTrack()) {
            m_playerController->next();
        }
        else {
            m_playerController->stop();
        }

        auto* worker = new DeleteWorker(m_library, tracks, mode);
        auto* thread = new QThread(this);
        worker->moveToThread(thread);

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
        auto* dialog = new DeleteDialog(tracks, mode, m_settings, Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dialog, &QDialog::accepted, dialog, [runDelete]() { runDelete(); });
        dialog->open();
    }
    else {
        runDelete();
    }
}

} // namespace Fooyin::ShortcutExtender
