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

#include "fileopsbridge.h"

#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/worker.h>

#include <QObject>

#include <vector>

namespace Fooyin {
class MusicLibrary;
class SettingsManager;

namespace ShortcutExtender {

struct FileOpsItem
{
    FileOpsOperation op;
    QString          name;
    QString          source;
    QString          destination;
};

// Background worker that mirrors FileOpsWorker (fileopsworker.cpp) in the
// fileops plugin.  Runs file operations on a dedicated thread and updates
// MusicLibrary afterwards.
class FileOpsExecutor : public Worker
{
    Q_OBJECT

public:
    FileOpsExecutor(MusicLibrary* library, TrackList tracks, QObject* parent = nullptr);

    // Produces (source, destination) pairs without touching the filesystem.
    // Call from any thread; result is returned synchronously.
    [[nodiscard]] std::vector<FileOpsItem> simulate(const FileOpPreset& preset);

    // Executes the operations previously built by simulate(), or builds and
    // executes in one step if simulate() was not called separately.
    // Must be invoked on the worker's thread (via QMetaObject::invokeMethod or
    // a queued connection after moving the object to a QThread).
    void execute(const FileOpPreset& preset);

signals:
    void operationFinished(const Fooyin::ShortcutExtender::FileOpsItem& item);
    void finished(int succeeded, int failed);

private:
    void buildOperations(const FileOpPreset& preset);

    void processMove(const FileOpPreset& preset);
    void processCopy(const FileOpPreset& preset);
    void processRename(const FileOpPreset& preset);

    [[nodiscard]] QString evaluatePath(const QString& pathTemplate, const Track& track);

    MusicLibrary* m_library;
    TrackList     m_tracks;
    ScriptParser  m_scriptParser;

    // Built by simulate() / buildOperations(); consumed by execute()
    std::vector<FileOpsItem> m_operations;
    TrackList                m_tracksToUpdate;
    TrackList                m_tracksToDelete;
};

} // namespace ShortcutExtender
} // namespace Fooyin
