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

#include "fileopsexecutor.h"

#include "fileopsscriptenvironment.h"

#include <core/library/musiclibrary.h>
#include <core/scripting/scriptparser.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(FILEOPS_SC, "fy.shortcutextender.fileops")

using namespace Qt::StringLiterals;

namespace Fooyin::ShortcutExtender {

FileOpsExecutor::FileOpsExecutor(MusicLibrary* library, TrackList tracks, QObject* parent)
    : Worker{parent}
    , m_library{library}
    , m_tracks{std::move(tracks)}
{ }

std::vector<FileOpsItem> FileOpsExecutor::simulate(const FileOpPreset& preset)
{
    m_operations.clear();
    m_tracksToUpdate.clear();
    m_tracksToDelete.clear();
    buildOperations(preset);
    return m_operations;
}

void FileOpsExecutor::execute(const FileOpPreset& preset)
{
    setState(Running);

    if(m_operations.empty()) {
        buildOperations(preset);
    }

    int succeeded{0};
    int failed{0};

    for(const FileOpsItem& item : m_operations) {
        if(!mayRun()) {
            break;
        }

        switch(item.op) {
            case FileOpsOperation::Create: {
                if(!QDir{}.mkpath(item.destination)) {
                    qCWarning(FILEOPS_SC) << "Failed to create directory" << item.destination;
                    ++failed;
                }
                break;
            }
            case FileOpsOperation::Remove: {
                if(!QDir{item.source}.rmdir(item.source)) {
                    qCWarning(FILEOPS_SC) << "Failed to remove directory" << item.source;
                    ++failed;
                }
                break;
            }
            case FileOpsOperation::Move:
            case FileOpsOperation::Rename: {
                QFile f{item.source};
                if(!f.exists()) {
                    qCWarning(FILEOPS_SC) << "Source does not exist:" << item.source;
                    ++failed;
                    break;
                }
                if(!f.rename(item.destination)) {
                    qCWarning(FILEOPS_SC) << "Failed to move/rename" << item.source << "->" << item.destination;
                    ++failed;
                    break;
                }
                ++succeeded;
                break;
            }
            case FileOpsOperation::Copy: {
                QFile f{item.source};
                if(!f.exists()) {
                    qCWarning(FILEOPS_SC) << "Source does not exist:" << item.source;
                    ++failed;
                    break;
                }
                if(!f.copy(item.destination)) {
                    qCWarning(FILEOPS_SC) << "Failed to copy" << item.source << "->" << item.destination;
                    ++failed;
                    break;
                }
                ++succeeded;
                break;
            }
            case FileOpsOperation::Delete:
                break;
        }

        emit operationFinished(item);
    }

    // Update library for moved/renamed tracks
    if(!m_tracksToUpdate.empty()) {
        m_library->updateTrackMetadata(m_tracksToUpdate);
    }
    if(!m_tracksToDelete.empty()) {
        m_library->deleteTracks(m_tracksToDelete);
    }

    setState(Idle);
    emit finished(succeeded, failed);
    emit Worker::finished();
}

void FileOpsExecutor::buildOperations(const FileOpPreset& preset)
{
    switch(preset.op) {
        case FileOpsOperation::Copy:
            processCopy(preset);
            break;
        case FileOpsOperation::Move:
            processMove(preset);
            break;
        case FileOpsOperation::Rename:
            processRename(preset);
            break;
        default:
            break;
    }
}

void FileOpsExecutor::processMove(const FileOpPreset& preset)
{
    const QString pathTemplate = preset.dest + u"/"_s + preset.filename + u".%extension%"_s;

    std::set<QString> sourcePaths;
    std::set<QString> processed;

    // Build a filepath→track map from the full library (needed for wholeDir)
    std::unordered_multimap<QString, Track> trackPaths;
    for(const Track& t : m_library->tracks()) {
        trackPaths.emplace(t.filepath(), t);
    }

    for(const Track& track : m_tracks) {
        const QString srcPath = track.path();
        if(processed.contains(track.filepath()) || sourcePaths.contains(srcPath)) {
            continue;
        }
        processed.emplace(track.filepath());

        const QString destFilepath = QDir::cleanPath(evaluatePath(pathTemplate, track));
        if(track.filepath() == destFilepath) {
            continue;
        }

        const QString destPath = QFileInfo{destFilepath}.absolutePath();

        if(preset.wholeDir) {
            sourcePaths.emplace(srcPath);
            const QDir srcDir{srcPath};
            const QStringList files = Utils::File::getFilesInDirRecursive(srcPath);

            for(const QString& file : files) {
                const QString relativePath = srcDir.relativeFilePath(file);
                const QString fileDestPath = QDir::cleanPath(destPath + u"/"_s + relativePath);
                const QString parentPath   = QFileInfo{fileDestPath}.absolutePath();

                // Ensure destination directory exists in operations
                m_operations.push_back({FileOpsOperation::Create, {}, {}, parentPath});

                if(trackPaths.contains(file)) {
                    const Track& fileTrack = trackPaths.equal_range(file).first->second;
                    const QString fileDest = QDir::cleanPath(evaluatePath(pathTemplate, fileTrack));
                    m_operations.push_back({FileOpsOperation::Move, fileTrack.filenameExt(),
                                            fileTrack.filepath(), fileDest});
                    // Track library update
                    Track updated = fileTrack;
                    updated.setFilePath(fileDest);
                    m_tracksToUpdate.push_back(updated);
                }
                else {
                    const QFileInfo info{file};
                    m_operations.push_back({FileOpsOperation::Move, info.fileName(), file, fileDestPath});
                }
            }

            if(preset.removeEmpty) {
                m_operations.push_back({FileOpsOperation::Remove, srcDir.dirName(), srcPath, {}});
            }
        }
        else {
            m_operations.push_back({FileOpsOperation::Create, {}, {}, destPath});
            m_operations.push_back({FileOpsOperation::Move, track.filenameExt(), track.filepath(), destFilepath});

            // Update track path in library
            if(trackPaths.contains(track.filepath())) {
                auto range = trackPaths.equal_range(track.filepath());
                for(auto it = range.first; it != range.second; ++it) {
                    Track updated = it->second;
                    updated.setFilePath(destFilepath);
                    m_tracksToUpdate.push_back(updated);
                }
            }
        }
    }
}

void FileOpsExecutor::processCopy(const FileOpPreset& preset)
{
    const QString pathTemplate = preset.dest + u"/"_s + preset.filename + u".%extension%"_s;

    std::set<QString> sourcePaths;
    std::set<QString> processed;

    std::unordered_multimap<QString, Track> trackPaths;
    for(const Track& t : m_library->tracks()) {
        trackPaths.emplace(t.filepath(), t);
    }

    for(const Track& track : m_tracks) {
        const QString srcPath = track.path();
        if(processed.contains(track.filepath()) || sourcePaths.contains(srcPath)) {
            continue;
        }
        processed.emplace(track.filepath());

        const QString destFilepath = QDir::cleanPath(evaluatePath(pathTemplate, track));
        if(track.filepath() == destFilepath) {
            continue;
        }

        const QString destPath = QFileInfo{destFilepath}.absolutePath();

        if(preset.wholeDir) {
            sourcePaths.emplace(srcPath);
            const QDir srcDir{srcPath};
            const QStringList files = Utils::File::getFilesInDirRecursive(srcPath);

            for(const QString& file : files) {
                const QString relativePath = srcDir.relativeFilePath(file);
                const QString fileDestPath = QDir::cleanPath(destPath + u"/"_s + relativePath);
                const QString parentPath   = QFileInfo{fileDestPath}.absolutePath();

                m_operations.push_back({FileOpsOperation::Create, {}, {}, parentPath});

                if(trackPaths.contains(file)) {
                    const Track& fileTrack = trackPaths.equal_range(file).first->second;
                    m_operations.push_back({FileOpsOperation::Copy, fileTrack.filenameExt(),
                                            fileTrack.filepath(), fileDestPath});
                }
                else {
                    const QFileInfo info{file};
                    m_operations.push_back({FileOpsOperation::Copy, info.fileName(), file, fileDestPath});
                }
            }
        }
        else {
            m_operations.push_back({FileOpsOperation::Create, {}, {}, destPath});
            m_operations.push_back({FileOpsOperation::Copy, track.filenameExt(), track.filepath(), destFilepath});
        }
    }
}

void FileOpsExecutor::processRename(const FileOpPreset& preset)
{
    static const QRegularExpression pathSepRe{uR"([/\\])"_s};

    const QString pathTemplate = preset.filename + u".%extension%"_s;

    std::set<QString> processed;

    for(const Track& track : m_tracks) {
        if(processed.contains(track.filepath())) {
            continue;
        }
        processed.emplace(track.filepath());

        // evaluatePath uses the script environment's replacePathSeparators=true,
        // but for Rename the result is a filename, not a full path, so we apply
        // an extra explicit replacement to be safe (mirrors replaceSeparators()
        // in fileopsworker.cpp).
        QString destFilename = evaluatePath(pathTemplate, track);
        destFilename.replace(pathSepRe, u"-"_s);
        destFilename = QDir::cleanPath(destFilename);

        if(track.filenameExt() == destFilename) {
            continue;
        }

        const QString destFilepath = QDir::cleanPath(track.path() + u"/"_s + destFilename);
        m_operations.push_back({FileOpsOperation::Rename, track.filenameExt(), track.filepath(), destFilepath});

        // Schedule the renamed path for library update
        Track updated = track;
        updated.setFilePath(destFilepath);
        m_tracksToUpdate.push_back(updated);
    }
}

QString FileOpsExecutor::evaluatePath(const QString& pathTemplate, const Track& track)
{
    static const FileOpsScriptEnvironment environment;
    const ParsedScript script   = m_scriptParser.parse(pathTemplate);
    const ScriptContext context{.environment = &environment};
    return m_scriptParser.evaluate(script, track, context);
}

} // namespace Fooyin::ShortcutExtender
