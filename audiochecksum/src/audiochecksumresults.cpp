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

#include "audiochecksumresults.h"

#include "audiochecksumdefs.h"
#include "audiochecksumresultsmodel.h"

#include <core/library/musiclibrary.h>
#include <utils/stringutils.h>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

AudioChecksumResults::AudioChecksumResults(MusicLibrary* library,
                                           QList<ChecksumResult> results,
                                           std::chrono::milliseconds timeTaken,
                                           QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_resultsView{new QTableView(this)}
    , m_resultsModel{new AudioChecksumResultsModel(std::move(results), this)}
    , m_proxyModel{new QSortFilterProxyModel(this)}
    , m_status{new QLabel(tr("Time taken") + ": "_L1 + Utils::msToString(timeTaken, false), this)}
    , m_buttonBox{new QDialogButtonBox(this)}
{
    setWindowTitle(tr("Audio Checksum Results"));
    setModal(false);

    m_proxyModel->setSourceModel(m_resultsModel);

    m_resultsView->setModel(m_proxyModel);
    m_resultsView->setSortingEnabled(true);
    m_resultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resultsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsView->verticalHeader()->hide();
    m_resultsView->horizontalHeader()->setStretchLastSection(false);
    m_resultsView->horizontalHeader()->setSortIndicatorShown(true);
    m_resultsView->sortByColumn(static_cast<int>(AudioChecksumResultsModel::Column::Filename),
                                Qt::AscendingOrder);

    // Column sizing: filename stretches, the rest fit content
    m_resultsView->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(AudioChecksumResultsModel::Column::Filename),
        QHeaderView::Stretch);
    m_resultsView->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(AudioChecksumResultsModel::Column::Algorithm),
        QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(AudioChecksumResultsModel::Column::ComputedHash),
        QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(AudioChecksumResultsModel::Column::StoredHash),
        QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(AudioChecksumResultsModel::Column::Status),
        QHeaderView::ResizeToContents);

    // Show "Save to Tags" button only when there are results worth tagging
    const QList<ChecksumResult> toSave = m_resultsModel->resultsToSave();
    if(!toSave.isEmpty()) {
        auto* saveButton = m_buttonBox->addButton(tr("&Save to Tags"),
                                                  QDialogButtonBox::AcceptRole);
        QObject::connect(saveButton, &QPushButton::clicked, this,
                         &AudioChecksumResults::accept);
        m_buttonBox->addButton(QDialogButtonBox::Cancel);
        QObject::connect(m_buttonBox, &QDialogButtonBox::rejected,
                         this, &QDialog::reject);
    }
    else {
        m_buttonBox->addButton(QDialogButtonBox::Close);
        QObject::connect(m_buttonBox, &QDialogButtonBox::rejected,
                         this, &QDialog::reject);
        // "Close" sends rejected
        QObject::connect(m_buttonBox, &QDialogButtonBox::accepted,
                         this, &QDialog::accept);
    }

    setupContextMenu();

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_resultsView, 0, 0, 1, 2);
    layout->addWidget(m_status, 1, 0);
    layout->addWidget(m_buttonBox, 1, 1);
    layout->setColumnStretch(0, 1);
}

void AudioChecksumResults::accept()
{
    QList<ChecksumResult> toSave = m_resultsModel->resultsToSave();
    if(toSave.isEmpty()) {
        QDialog::accept();
        return;
    }

    // Apply computed hash to the AUDIOCHECKSUM tag before writing
    TrackList tracks;
    tracks.reserve(toSave.size());
    for(auto& result : toSave) {
        result.track.replaceExtraTag(TagFieldName, result.computedHash);
        tracks.push_back(result.track);
    }

    m_status->setText(tr("Writing to file tags…"));
    // Disable the accept-role button while writing
    const auto buttons = m_buttonBox->buttons();
    for(auto* btn : buttons) {
        if(m_buttonBox->buttonRole(btn) == QDialogButtonBox::AcceptRole)
            btn->setEnabled(false);
    }

    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged,
                     this, [this]() { QDialog::accept(); },
                     Qt::SingleShotConnection);

    const auto request = m_library->writeTrackMetadata(tracks);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected,
                     this, [request]() { request.cancel(); },
                     Qt::SingleShotConnection);
}

void AudioChecksumResults::setupContextMenu()
{
    m_resultsView->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_resultsView, &QTableView::customContextMenuRequested,
                     this, [this](const QPoint& pos) {
                         const QModelIndex proxyIndex = m_resultsView->indexAt(pos);
                         if(!proxyIndex.isValid())
                             return;

                         const QModelIndex srcIndex =
                             m_proxyModel->mapToSource(proxyIndex);
                         const ChecksumResult& result =
                             m_resultsModel->results().at(srcIndex.row());

                         QMenu menu{this};

                         auto* copyComputed =
                             menu.addAction(tr("Copy Computed Checksum"));
                         auto* copyStored =
                             menu.addAction(tr("Copy Stored Checksum"));
                         copyStored->setEnabled(!result.storedHash.isEmpty());

                         QObject::connect(copyComputed, &QAction::triggered, this,
                                          [&result]() {
                                              QApplication::clipboard()->setText(
                                                  result.computedHash);
                                          });
                         QObject::connect(copyStored, &QAction::triggered, this,
                                          [&result]() {
                                              QApplication::clipboard()->setText(
                                                  result.storedHash);
                                          });

                         menu.exec(m_resultsView->viewport()->mapToGlobal(pos));
                     });
}

QSize AudioChecksumResults::sizeHint() const
{
    QSize size = m_resultsView->sizeHint();
    size.rheight() += 200;
    size.rwidth() += 600;
    return size;
}

QSize AudioChecksumResults::minimumSizeHint() const
{
    return QDialog::minimumSizeHint();
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumresults.cpp"
