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
bool DeleteWorker::moveToXdgTrash(const QString& filepath)
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if(dataHome.isEmpty()) {
        return false;
    }

    const QDir trashFiles{dataHome + u"/Trash/files"_s};
    const QDir trashInfo{dataHome + u"/Trash/info"_s};

    if(!trashFiles.mkpath(u"."_s) || !trashInfo.mkpath(u"."_s)) {
        qCWarning(SHORTCUTEXT) << "Failed to create Trash directories under" << dataHome;
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

    const QString trashFilePath  = trashFiles.filePath(destName);
    const QString trashInfoPath  = trashInfo.filePath(destName + u".trashinfo"_s);
    const QString deletionDate   = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Write .trashinfo before moving so the metadata is present if anything fails mid-way.
    {
        QSaveFile infoFile{trashInfoPath};
        if(!infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCWarning(SHORTCUTEXT) << "Failed to open trash info file for writing:" << trashInfoPath;
            return false;
        }
        const QString infoContents = u"[Trash Info]\nPath=%1\nDeletionDate=%2\n"_s
                                         .arg(filepath)
                                         .arg(deletionDate);
        infoFile.write(infoContents.toUtf8());
        if(!infoFile.commit()) {
            qCWarning(SHORTCUTEXT) << "Failed to write trash info file:" << trashInfoPath;
            return false;
        }
    }

    // Attempt a fast rename (works only when source and trash are on the same filesystem).
    if(QFile::rename(filepath, trashFilePath)) {
        return true;
    }

    // Cross-filesystem fallback: remove the .trashinfo we just wrote and let Qt handle it.
    QFile::remove(trashInfoPath);
    qCDebug(SHORTCUTEXT) << "Rename to trash failed (cross-filesystem?), falling back to QFile::moveToTrash";
    return QFile::moveToTrash(filepath);
}

void DeleteWorker::deleteFiles()
{
    setState(Running);

    TrackList deleted;
    std::set<QString> processed;

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            break;
        }

        const QString filepath = track.filepath();
        if(processed.contains(filepath)) {
            continue;
        }
        processed.emplace(filepath);

        bool ok = false;
        if(m_mode == DeleteMode::Trash) {
            ok = moveToXdgTrash(filepath);
        }
        else {
            ok = QFile::remove(filepath);
        }

        if(!ok) {
            qCWarning(SHORTCUTEXT) << "Failed to delete file:" << filepath;
            continue;
        }

        deleted.push_back(track);
    }

    if(!deleted.empty()) {
        m_library->deleteTracks(deleted);
    }

    setState(Idle);

    emit deleteFinished(deleted);
    emit finished();
}

} // namespace Fooyin::ShortcutExtender
