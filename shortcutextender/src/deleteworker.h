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

#include "shortcutextenderdefs.h"

#include <core/track.h>
#include <utils/worker.h>

namespace Fooyin {
class MusicLibrary;

namespace ShortcutExtender {

class DeleteWorker : public Worker
{
    Q_OBJECT

public:
    explicit DeleteWorker(MusicLibrary* library, TrackList tracks, DeleteMode mode, QObject* parent = nullptr);

    void deleteFiles();

signals:
    void deleteFinished(const Fooyin::TrackList& deletedTracks);
    void trashError(const QString& message);

private:
    [[nodiscard]] bool moveToXdgTrash(const QString& filepath);

    MusicLibrary* m_library;
    TrackList     m_tracks;
    DeleteMode    m_mode;
};

} // namespace ShortcutExtender
} // namespace Fooyin
