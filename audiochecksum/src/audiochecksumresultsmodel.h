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

#include <QAbstractTableModel>
#include <QList>

namespace Fooyin::AudioChecksum {

class AudioChecksumResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        Filename = 0,
        Algorithm,
        ComputedHash,
        StoredHash,
        Status,
        Count  // sentinel — keep last
    };

    explicit AudioChecksumResultsModel(QList<ChecksumResult> results,
                                       QObject* parent = nullptr);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex{}) const override;

    void sort(int column, Qt::SortOrder order) override;

    /*! Replaces the entire result set and resets the view. */
    void setResults(QList<ChecksumResult> results);

    /*!
     * Returns those results whose status warrants writing a tag:
     *   - New      → write computed hash for the first time
     *   - Mismatch → update the stored hash to the newly computed value
     *
     * Results with Status::Match or Status::Error are excluded.
     */
    [[nodiscard]] QList<ChecksumResult> resultsToSave() const;

    [[nodiscard]] const QList<ChecksumResult>& results() const;

private:
    QList<ChecksumResult> m_results;
};

} // namespace Fooyin::AudioChecksum
