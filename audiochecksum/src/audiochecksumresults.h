/*
 * Fooyin AudioChecksum Plugin
 * Copyright © 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "audiochecksumresult.h"

#include <QDialog>
#include <QList>

class QDialogButtonBox;
class QLabel;
class QSortFilterProxyModel;
class QTableView;

namespace Fooyin {
class MusicLibrary;
} // namespace Fooyin

namespace Fooyin::AudioChecksum {

class AudioChecksumResultsModel;

/*!
 * Non-modal dialog showing a sortable table of checksum scan results.
 *
 * "Save to Tags" writes the computed checksum into the AUDIOCHECKSUM tag
 * for tracks with status New or Mismatch.
 */
class AudioChecksumResults : public QDialog
{
    Q_OBJECT

public:
    AudioChecksumResults(MusicLibrary* library,
                         QList<ChecksumResult> results,
                         std::chrono::milliseconds timeTaken,
                         QWidget* parent = nullptr);

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    void setupContextMenu();

    MusicLibrary* m_library;

    QTableView* m_resultsView;
    AudioChecksumResultsModel* m_resultsModel;
    QSortFilterProxyModel* m_proxyModel;
    QLabel* m_status;
    QDialogButtonBox* m_buttonBox;
};

} // namespace Fooyin::AudioChecksum
