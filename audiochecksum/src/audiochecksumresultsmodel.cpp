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

#include "audiochecksumresultsmodel.h"

#include <QFileInfo>
#include <QString>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

namespace {

QString statusToString(ChecksumResult::Status status)
{
    switch(status) {
        case ChecksumResult::Status::New:
            return QObject::tr("New");
        case ChecksumResult::Status::Match:
            return QObject::tr("Match");
        case ChecksumResult::Status::Mismatch:
            return QObject::tr("Mismatch");
        case ChecksumResult::Status::Error:
            return QObject::tr("Error");
    }
    return {};
}

} // namespace

AudioChecksumResultsModel::AudioChecksumResultsModel(QList<ChecksumResult> results,
                                                     QObject* parent)
    : QAbstractTableModel{parent}
    , m_results{std::move(results)}
{ }

QVariant AudioChecksumResultsModel::headerData(int section,
                                               Qt::Orientation orientation,
                                               int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch(static_cast<Column>(section)) {
        case Column::Filename:
            return tr("Filename");
        case Column::Algorithm:
            return tr("Algorithm");
        case Column::ComputedHash:
            return tr("Computed Checksum");
        case Column::StoredHash:
            return tr("Stored Checksum");
        case Column::Status:
            return tr("Status");
        default:
            return {};
    }
}

QVariant AudioChecksumResultsModel::data(const QModelIndex& index, int role) const
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
        case Column::Algorithm:
            return result.algorithm;
        case Column::ComputedHash:
            return result.computedHash;
        case Column::StoredHash:
            return result.storedHash;
        case Column::Status:
            if(role == Qt::ToolTipRole && result.status == ChecksumResult::Status::Error)
                return result.errorString;
            return statusToString(result.status);
        default:
            return {};
    }
}

int AudioChecksumResultsModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;
    return static_cast<int>(m_results.size());
}

int AudioChecksumResultsModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;
    return static_cast<int>(Column::Count);
}

void AudioChecksumResultsModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();

    std::sort(m_results.begin(), m_results.end(),
              [column, order](const ChecksumResult& a, const ChecksumResult& b) {
                  QString va, vb;
                  switch(static_cast<Column>(column)) {
                      case Column::Filename:
                          va = QFileInfo{a.track.filepath()}.fileName();
                          vb = QFileInfo{b.track.filepath()}.fileName();
                          break;
                      case Column::Algorithm:
                          va = a.algorithm;
                          vb = b.algorithm;
                          break;
                      case Column::ComputedHash:
                          va = a.computedHash;
                          vb = b.computedHash;
                          break;
                      case Column::StoredHash:
                          va = a.storedHash;
                          vb = b.storedHash;
                          break;
                      case Column::Status:
                          va = statusToString(a.status);
                          vb = statusToString(b.status);
                          break;
                      default:
                          break;
                  }
                  if(order == Qt::AscendingOrder)
                      return va < vb;
                  return va > vb;
              });

    endResetModel();
}

QList<ChecksumResult> AudioChecksumResultsModel::resultsToSave() const
{
    QList<ChecksumResult> toSave;
    for(const auto& result : m_results) {
        if(result.status != ChecksumResult::Status::New
           && result.status != ChecksumResult::Status::Mismatch)
            continue;

        // FLAC files carry an embedded STREAMINFO MD5 as their authoritative
        // checksum — we never write a tag for them.
        const QString codec = result.track.codec().toLower();
        if(codec == u"flac"
           || result.track.filepath().endsWith(u".flac", Qt::CaseInsensitive))
            continue;

        toSave.append(result);
    }
    return toSave;
}

const QList<ChecksumResult>& AudioChecksumResultsModel::results() const
{
    return m_results;
}

void AudioChecksumResultsModel::setResults(QList<ChecksumResult> results)
{
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumresultsmodel.cpp"
