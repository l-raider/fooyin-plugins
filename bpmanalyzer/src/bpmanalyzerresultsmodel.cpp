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

#include "bpmanalyzerresultsmodel.h"

#include "bpmanalyzerdefs.h"

#include <QFileInfo>
#include <QString>

#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin::BpmAnalyzer {

namespace {

// Scale a BPM string value by factor; returns empty string on parse failure.
QString scaleBpmString(const QString& bpmStr, float factor, int precision = 0)
{
    bool ok = false;
    const float val = bpmStr.toFloat(&ok);
    if(!ok || val <= 0.0f)
        return bpmStr;

    const float scaled = val * factor;
    switch(precision) {
        case 1: return QString::number(static_cast<double>(scaled), 'f', 1);
        case 2: return QString::number(static_cast<double>(scaled), 'f', 2);
        default: return QString::number(static_cast<int>(std::round(scaled)));
    }
}

} // namespace

BpmAnalyzerResultsModel::BpmAnalyzerResultsModel(QList<BpmResult> results,
                                                 QObject* parent)
    : QAbstractTableModel{parent}
    , m_results{std::move(results)}
{ }

QVariant BpmAnalyzerResultsModel::headerData(int section,
                                             Qt::Orientation orientation,
                                             int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch(static_cast<Column>(section)) {
        case Column::Filename:    return tr("Filename");
        case Column::AnalyzedBpm: return tr("Analyzed BPM");
        case Column::StoredBpm:   return tr("Stored BPM");
        default:                  return {};
    }
}

QVariant BpmAnalyzerResultsModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid())
        return {};
    if(role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};

    const auto& result = m_results.at(index.row());

    switch(static_cast<Column>(index.column())) {
        case Column::Filename:
            if(role == Qt::ToolTipRole)
                return result.track.filepath();
            return QFileInfo{result.track.filepath()}.fileName();
        case Column::AnalyzedBpm:
            return result.analyzedBpm;
        case Column::StoredBpm:
            return result.storedBpm;
        default:
            return {};
    }
}

int BpmAnalyzerResultsModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;
    return static_cast<int>(m_results.size());
}

int BpmAnalyzerResultsModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;
    return static_cast<int>(Column::Count);
}

void BpmAnalyzerResultsModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();

    std::sort(m_results.begin(), m_results.end(),
              [column, order](const BpmResult& a, const BpmResult& b) {
                  QString va, vb;
                  switch(static_cast<Column>(column)) {
                      case Column::Filename:
                          va = QFileInfo{a.track.filepath()}.fileName();
                          vb = QFileInfo{b.track.filepath()}.fileName();
                          break;
                      case Column::AnalyzedBpm:
                          va = a.analyzedBpm;
                          vb = b.analyzedBpm;
                          break;
                      case Column::StoredBpm:
                          va = a.storedBpm;
                          vb = b.storedBpm;
                          break;
                      default:
                          break;
                  }
                  return order == Qt::AscendingOrder ? (va < vb) : (va > vb);
              });

    endResetModel();
}

void BpmAnalyzerResultsModel::setResults(QList<BpmResult> results)
{
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
}

void BpmAnalyzerResultsModel::appendResult(const BpmResult& result)
{
    const int row = static_cast<int>(m_results.size());
    beginInsertRows(QModelIndex{}, row, row);
    m_results.append(result);
    endInsertRows();
}

void BpmAnalyzerResultsModel::scaleBpm(const QList<int>& sourceRows, float factor)
{
    for(int row : sourceRows) {
        if(row < 0 || row >= static_cast<int>(m_results.size()))
            continue;

        auto& result = m_results[row];
        if(result.analyzedBpm.isEmpty())
            continue;

        // Detect precision from the current string
        int precision = 0;
        if(result.analyzedBpm.contains(u'.')) {
            const int dotPos = result.analyzedBpm.lastIndexOf(u'.');
            precision = static_cast<int>(result.analyzedBpm.size()) - dotPos - 1;
        }

        result.analyzedBpm = scaleBpmString(result.analyzedBpm, factor, precision);

        const QModelIndex first = index(row, 0);
        const QModelIndex last  = index(row, columnCount() - 1);
        emit dataChanged(first, last);
    }
}

QList<BpmResult> BpmAnalyzerResultsModel::resultsToSave() const
{
    QList<BpmResult> toSave;
    for(const auto& result : m_results) {
        if(result.status == BpmResult::Status::New
           || result.status == BpmResult::Status::Updated) {
            toSave.append(result);
        }
    }
    return toSave;
}

void BpmAnalyzerResultsModel::markSaved()
{
    for(qsizetype i{0}; i < m_results.size(); ++i) {
        auto& result = m_results[i];
        if(result.status == BpmResult::Status::New
           || result.status == BpmResult::Status::Updated) {
            result.storedBpm = result.analyzedBpm;
            result.status    = BpmResult::Status::Updated;
            const QModelIndex first = index(static_cast<int>(i), 0);
            const QModelIndex last  = index(static_cast<int>(i), columnCount() - 1);
            emit dataChanged(first, last);
        }
    }
}

const QList<BpmResult>& BpmAnalyzerResultsModel::results() const
{
    return m_results;
}

} // namespace Fooyin::BpmAnalyzer
