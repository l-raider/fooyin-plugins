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

#include "deleteworker.h"

#include <core/library/musiclibrary.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(SHORTCUTEXT, "fy.shortcutextender")

using namespace Qt::StringLiterals;

namespace Fooyin::ShortcutExtender {

DeleteWorker::DeleteWorker(MusicLibrary* library, TrackList tracks, DeleteMode mode, QObject* parent)
    : Worker{parent}
    , m_library{library}
    , m_tracks{std::move(tracks)}
    , m_mode{mode}
{ }

// Manually implements the XDG Trash specification so that it works correctly
// inside a Flatpak sandbox (where QFile::moveToTrash() may use a portal that
// results in inaccessible or misplaced files).
//
// Steps:
//   1. Resolve a unique base-name in ~/.local/share/Trash/files/.
//   2. Write a matching .trashinfo file into ~/.local/share/Trash/info/.
//   3. Rename the source file into Trash/files/ (atomic on same filesystem).
//
// If the rename fails (source is on a different filesystem than $HOME),
// fall back to QFile::moveToTrash() which handles cross-device moves.
//
// Flatpak note: inside a Flatpak sandbox QStandardPaths::GenericDataLocation
// resolves to the app-private container (~/.var/app/<id>/data) rather than the
// real ~/.local/share.  Files trashed there are invisible to the host desktop
// and cannot be restored through any file manager.  We detect the sandbox via
// the FLATPAK_ID environment variable and, since fooyin's manifest grants the
// 'home' filesystem permission, use $HOME/.local/share directly so that trashed
// files end up in the host user's trash.
static QString xdgDataHome()
{
    if(qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return QDir::homePath() + u"/.local/share"_s;
    }
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

bool DeleteWorker::moveToXdgTrash(const QString& filepath)
{
    const QString dataHome = xdgDataHome();
    if(dataHome.isEmpty()) {
        const QString msg = tr("Could not determine a writable data directory. Unable to move to trash:\n%1").arg(filepath);
        qCCritical(SHORTCUTEXT) << msg;
        emit trashError(msg);
        return false;
    }

    const QDir trashFiles{dataHome + u"/Trash/files"_s};
    const QDir trashInfo{dataHome + u"/Trash/info"_s};

    if(!trashFiles.mkpath(u"."_s) || !trashInfo.mkpath(u"."_s)) {
        const QString msg = tr("No write permission to the trash directory:\n%1\n\n"
                               "To delete files permanently instead, open Settings and "
                               "change \"Delete mode\" to \"Delete permanently\" under "
                               "Shortcut Extender.").arg(trashFiles.path());
        qCCritical(SHORTCUTEXT) << "No write permission to Trash directories under" << dataHome
                                << "— unable to trash:" << filepath;
        emit trashError(msg);
        return false;
    }

    const QFileInfo srcInfo{filepath};
    const QString   baseName  = srcInfo.fileName();
    const QString   extension = srcInfo.suffix().isEmpty() ? QString{} : u"."_s + srcInfo.suffix();
    const QString   stem      = srcInfo.suffix().isEmpty() ? baseName : srcInfo.completeBaseName();

    // Find a unique destination name (handles collisions).
    QString destName = baseName;
    int     counter  = 1;
    while(QFile::exists(trashFiles.filePath(destName))
          || QFile::exists(trashInfo.filePath(destName + u".trashinfo"_s))) {
        destName = u"%1_%2%3"_s.arg(stem).arg(counter).arg(extension);
        ++counter;
    }

    const QString trashFilePath = trashFiles.filePath(destName);
    const QString trashInfoPath = trashInfo.filePath(destName + u".trashinfo"_s);
    const QString deletionDate  = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Write .trashinfo before moving so the metadata is present if anything fails mid-way.
    {
        QSaveFile infoFile{trashInfoPath};
        if(!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const QString msg = tr("No write permission to the trash directory:\n%1\n\n"
                                   "To delete files permanently instead, open Settings and "
                                   "change \"Delete mode\" to \"Delete permanently\" under "
                                   "Shortcut Extender.").arg(trashInfo.path());
            qCCritical(SHORTCUTEXT) << "No write permission to trash info directory" << trashInfo.path()
                                    << "— unable to trash:" << filepath;
            emit trashError(msg);
            return false;
        }
        const QString infoContents = u"[Trash Info]\nPath=%1\nDeletionDate=%2\n"_s
                                         .arg(filepath)
                                         .arg(deletionDate);
        infoFile.write(infoContents.toUtf8());
        if(!infoFile.commit()) {
            qCWarning(SHORTCUTEXT) << "Failed to write trash info file" << trashInfoPath
                                   << "— unable to trash:" << filepath;
            return false;
        }
    }

    // Attempt a fast rename (works only when source and trash are on the same filesystem).
    if(QFile::rename(filepath, trashFilePath)) {
        qCInfo(SHORTCUTEXT) << "Trashed:" << filepath << "->" << trashFilePath;
        return true;
    }

    // Cross-filesystem fallback: remove the .trashinfo we just wrote and let Qt handle it.
    QFile::remove(trashInfoPath);
    qCDebug(SHORTCUTEXT) << "Same-filesystem rename failed; falling back to QFile::moveToTrash for:" << filepath;

    if(QFile::moveToTrash(filepath)) {
        qCInfo(SHORTCUTEXT) << "Trashed:" << filepath;
        return true;
    }

    const QString msg = tr("Failed to move file to trash:\n%1\n\n"
                           "To delete files permanently instead, open Settings and "
                           "change \"Delete mode\" to \"Delete permanently\" under "
                           "Shortcut Extender.").arg(filepath);
    qCCritical(SHORTCUTEXT) << "All trash strategies failed for:" << filepath;
    emit trashError(msg);
    return false;
}

void DeleteWorker::deleteFiles()
{
    setState(Running);

    TrackList deleted;
    std::set<QString> processed;

    qCDebug(SHORTCUTEXT) << "Starting delete for" << m_tracks.size() << "track(s),"
                         << "mode:" << (m_mode == DeleteMode::Trash ? "trash" : "permanent");

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            qCDebug(SHORTCUTEXT) << "Delete interrupted by stop request";
            break;
        }

        const QString filepath = track.filepath();
        if(processed.contains(filepath)) {
            qCDebug(SHORTCUTEXT) << "Skipping duplicate path:" << filepath;
            continue;
        }
        processed.emplace(filepath);

        bool ok = false;
        if(m_mode == DeleteMode::Trash) {
            ok = moveToXdgTrash(filepath);
        }
        else {
            ok = QFile::remove(filepath);
            if(ok) {
                qCInfo(SHORTCUTEXT) << "Permanently deleted:" << filepath;
            }
            else {
                qCCritical(SHORTCUTEXT) << "Failed to permanently delete:" << filepath;
            }
        }

        if(!ok) {
            continue;
        }

        deleted.push_back(track);
    }

    if(!deleted.empty()) {
        qCInfo(SHORTCUTEXT) << "Removed" << deleted.size() << "track(s) from library";
        m_library->deleteTracks(deleted);
    }
    else {
        qCWarning(SHORTCUTEXT) << "Delete operation completed but no files were successfully deleted";
    }

    setState(Idle);

    emit deleteFinished(deleted);
    emit finished();
}

} // namespace Fooyin::ShortcutExtender
