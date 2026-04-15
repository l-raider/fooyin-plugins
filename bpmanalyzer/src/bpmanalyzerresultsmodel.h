/*
 * Fooyin BPM Analyzer Plugin
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

#include "bpmanalyzerresult.h"

#include <QAbstractTableModel>
#include <QList>

namespace Fooyin::BpmAnalyzer {

class BpmAnalyzerResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column
    {
        Filename    = 0,
        AnalyzedBpm,
        StoredBpm,
        Count  // sentinel — keep last
    };

    explicit BpmAnalyzerResultsModel(QList<BpmResult> results,
                                     QObject* parent = nullptr);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex{}) const override;

    void sort(int column, Qt::SortOrder order) override;

    /*! Replaces the entire result set and resets the view. */
    void setResults(QList<BpmResult> results);

    /*! Appends a single result incrementally during scanning. */
    void appendResult(const BpmResult& result);

    /*!
     * Scales the analyzed BPM for the given source-model rows by factor.
     * Used to implement Double BPM (factor=2.0) and Halve BPM (factor=0.5).
     * Only modifies the in-memory model; does NOT write any tags.
     */
    void scaleBpm(const QList<int>& sourceRows, float factor);

    /*!
     * Returns results that should be written to tags:
     *   - Status::New     → fresh analysis, no existing tag
     *   - Status::Updated → existing tag will be overwritten
     * Skipped and Error rows are excluded.
     */
    [[nodiscard]] QList<BpmResult> resultsToSave() const;

    /*! Transition New/Updated rows to Updated (storedBpm ← analyzedBpm) after save. */
    void markSaved();

    [[nodiscard]] const QList<BpmResult>& results() const;

private:
    QList<BpmResult> m_results;
};

} // namespace Fooyin::BpmAnalyzer
