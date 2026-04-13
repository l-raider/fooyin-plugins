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
#include "audiochecksumscanner.h"

#include <core/library/musiclibrary.h>
#include <utils/stringutils.h>

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

AudioChecksumResults::AudioChecksumResults(MusicLibrary* library,
                                           std::shared_ptr<AudioLoader> audioLoader,
                                           TrackList tracks,
                                           QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_tracks{std::move(tracks)}
    , m_resultsView{new QTableView(this)}
    , m_resultsModel{new AudioChecksumResultsModel({}, this)}
    , m_proxyModel{new QSortFilterProxyModel(this)}
    , m_status{new QLabel(
          tr("Ready — %1 track(s) selected.").arg(static_cast<int>(m_tracks.size())), this)}
    , m_progressBar{new QProgressBar(this)}
    , m_calcButton{new QPushButton(tr("&Calculate && Verify"), this)}
    , m_saveButton{new QPushButton(tr("&Save to Tags"), this)}
    , m_closeButton{new QPushButton(tr("Close"), this)}
{
    setWindowTitle(tr("Audio Checksum"));
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
    m_resultsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    m_saveButton->setEnabled(false);
    m_closeButton->setDefault(true);

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);

    QObject::connect(m_calcButton, &QPushButton::clicked, this,
                     [this]() { startScan(); });
    QObject::connect(m_saveButton, &QPushButton::clicked, this,
                     &AudioChecksumResults::saveToTags);
    QObject::connect(m_closeButton, &QPushButton::clicked, this,
                     &QDialog::close);

    setupContextMenu();

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_calcButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_closeButton);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_resultsView, 0, 0);
    layout->addWidget(m_progressBar, 1, 0);
    layout->addWidget(m_status, 2, 0);
    layout->addLayout(buttonLayout, 3, 0);
    layout->setRowStretch(0, 1);
}

void AudioChecksumResults::startScan()
{
    if(m_scanning)
        return;

    m_scanning = true;
    m_calcButton->setEnabled(false);
    m_saveButton->setEnabled(false);

    const int total = static_cast<int>(m_tracks.size());
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_status->setText(tr("Scanning…"));

    m_resultsModel->setResults({});
    m_scanStart = std::chrono::steady_clock::now();

    m_scanner = new AudioChecksumScanner(m_audioLoader, this);

    QObject::connect(m_scanner, &AudioChecksumScanner::scanningTrack, this,
                     [this, total](const QString& filepath) {
                         const int done = m_progressBar->value() + 1;
                         m_progressBar->setValue(done);
                         m_status->setText(
                             tr("Scanning %1 / %2: %3").arg(done).arg(total)
                                 .arg(QFileInfo{filepath}.fileName()));
                     });

    QObject::connect(m_scanner, &AudioChecksumScanner::scanFinished, this,
                     &AudioChecksumResults::onScanFinished);

    m_scanner->scanTracks(m_tracks);
}

void AudioChecksumResults::onScanFinished(const QList<ChecksumResult>& results)
{
    m_scanning = false;
    m_scanner->close();
    m_scanner = nullptr;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_scanStart);

    m_resultsModel->setResults(results);
    m_resultsView->resizeColumnsToContents();

    m_progressBar->setVisible(false);
    m_status->setText(tr("Time taken") + ": "_L1 + Utils::msToString(elapsed, false));

    m_calcButton->setEnabled(true);
    updateSaveButton();
}

void AudioChecksumResults::saveToTags()
{
    QList<ChecksumResult> toSave = m_resultsModel->resultsToSave();
    if(toSave.isEmpty())
        return;

    TrackList tracks;
    tracks.reserve(toSave.size());
    for(auto& result : toSave) {
        result.track.replaceExtraTag(TagFieldName, result.computedHash);
        tracks.push_back(result.track);
    }

    m_status->setText(tr("Writing to file tags…"));
    m_saveButton->setEnabled(false);

    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged,
                     this, [this]() {
                         m_status->setText(tr("Tags saved."));
                         updateSaveButton();
                     },
                     Qt::SingleShotConnection);

    m_library->writeTrackMetadata(tracks);
}

void AudioChecksumResults::updateSaveButton()
{
    m_saveButton->setEnabled(!m_resultsModel->resultsToSave().isEmpty());
}

void AudioChecksumResults::closeEvent(QCloseEvent* event)
{
    if(m_scanning && m_scanner) {
        m_scanner->close();
        m_scanner = nullptr;
        m_scanning = false;
    }
    QDialog::closeEvent(event);
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

                         auto* openFolder =
                             menu.addAction(tr("Open Containing Folder"));
                         menu.addSeparator();
                         auto* copyComputed =
                             menu.addAction(tr("Copy Computed Checksum"));
                         auto* copyStored =
                             menu.addAction(tr("Copy Stored Checksum"));
                         copyStored->setEnabled(!result.storedHash.isEmpty());

                         QObject::connect(openFolder, &QAction::triggered, this,
                                          [&result]() {
                                              const QString dir = QFileInfo{result.track.filepath()}.absolutePath();
                                              QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
                                          });

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
