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

#include "bpmanalyzerresults.h"

#include "bpmanalyzerdefs.h"
#include "bpmanalyzerresultsmodel.h"
#include "bpmanalyzerscanner.h"

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
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::BpmAnalyzer {

BpmAnalyzerResults::BpmAnalyzerResults(MusicLibrary* library,
                                       std::shared_ptr<AudioLoader> audioLoader,
                                       TrackList tracks,
                                       QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_tracks{std::move(tracks)}
    , m_resultsView{new QTableView(this)}
    , m_resultsModel{new BpmAnalyzerResultsModel({}, this)}
    , m_proxyModel{new QSortFilterProxyModel(this)}
    , m_status{new QLabel(
          tr("Ready — %1 track(s) selected.").arg(static_cast<int>(m_tracks.size())), this)}
    , m_progressBar{new QProgressBar(this)}
    , m_analyzeButton{new QPushButton(tr("&Analyze"), this)}
    , m_doubleBpmButton{new QPushButton(tr("×2 Double BPM"), this)}
    , m_halveBpmButton{new QPushButton(tr("½ Halve BPM"), this)}
    , m_saveButton{new QPushButton(tr("&Save to Tags"), this)}
    , m_closeButton{new QPushButton(tr("Close"), this)}
{
    setWindowTitle(tr("BPM Analyzer"));
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
    m_resultsView->sortByColumn(static_cast<int>(BpmAnalyzerResultsModel::Column::Filename),
                                Qt::AscendingOrder);
    m_resultsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    m_doubleBpmButton->setEnabled(false);
    m_halveBpmButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_closeButton->setDefault(true);

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);

    QObject::connect(m_analyzeButton, &QPushButton::clicked,
                     this, [this]() { startScan(); });
    QObject::connect(m_doubleBpmButton, &QPushButton::clicked,
                     this, [this]() { scaleSelectedBpm(2.0f); });
    QObject::connect(m_halveBpmButton, &QPushButton::clicked,
                     this, [this]() { scaleSelectedBpm(0.5f); });
    QObject::connect(m_saveButton, &QPushButton::clicked,
                     this, &BpmAnalyzerResults::saveToTags);
    QObject::connect(m_closeButton, &QPushButton::clicked,
                     this, &QDialog::close);

    // Enable Double/Halve buttons only when rows are selected
    QObject::connect(m_resultsView->selectionModel(),
                     &QItemSelectionModel::selectionChanged,
                     this, [this]() {
                         const bool has = m_resultsView->selectionModel()->hasSelection();
                         m_doubleBpmButton->setEnabled(has);
                         m_halveBpmButton->setEnabled(has);
                     });

    setupContextMenu();

    auto* scaleLayout = new QHBoxLayout;
    scaleLayout->setContentsMargins(0, 0, 0, 0);
    scaleLayout->addWidget(m_doubleBpmButton);
    scaleLayout->addWidget(m_halveBpmButton);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_analyzeButton);
    buttonLayout->addLayout(scaleLayout);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_closeButton);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_resultsView, 0, 0);
    layout->addWidget(m_progressBar, 1, 0);
    layout->addWidget(m_status,      2, 0);
    layout->addLayout(buttonLayout,  3, 0);
    layout->setRowStretch(0, 1);
}

void BpmAnalyzerResults::startScan()
{
    if(m_scanning)
        return;

    m_scanning = true;
    m_analyzeButton->setEnabled(false);
    m_saveButton->setEnabled(false);

    const int total = static_cast<int>(m_tracks.size());
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_status->setText(tr("Analyzing…"));

    m_resultsModel->setResults({});
    m_scanStart = std::chrono::steady_clock::now();

    m_scanner = new BpmAnalyzerScanner(m_audioLoader, this);

    QObject::connect(m_scanner, &BpmAnalyzerScanner::scanningTrack, this,
                     [this, total](const QString& filepath) {
                         const int done = m_progressBar->value() + 1;
                         m_progressBar->setValue(done);
                         m_status->setText(
                             tr("Analyzing %1 / %2: %3")
                                 .arg(done).arg(total)
                                 .arg(QFileInfo{filepath}.fileName()));
                     });

    QObject::connect(m_scanner, &BpmAnalyzerScanner::trackScanned, this,
                     [this](const BpmResult& result) {
                         m_resultsModel->appendResult(result);
                         m_resultsView->resizeColumnsToContents();
                     });

    QObject::connect(m_scanner, &BpmAnalyzerScanner::scanFinished,
                     this, &BpmAnalyzerResults::onScanFinished);

    m_scanner->scanTracks(m_tracks);
}

void BpmAnalyzerResults::onScanFinished(const QList<BpmResult>& /*results*/)
{
    m_scanning = false;
    if(m_scanner) {
        m_scanner->deleteLater();
        m_scanner = nullptr;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_scanStart);

    m_resultsView->resizeColumnsToContents();
    m_progressBar->setVisible(false);
    m_status->setText(tr("Time taken") + ": "_L1 + Utils::msToString(elapsed, false));

    m_analyzeButton->setEnabled(true);
    updateButtons();
}

void BpmAnalyzerResults::scaleSelectedBpm(float factor)
{
    const QModelIndexList selected = m_resultsView->selectionModel()->selectedRows();
    if(selected.isEmpty())
        return;

    QList<int> sourceRows;
    sourceRows.reserve(selected.size());
    for(const QModelIndex& proxyIdx : selected) {
        const QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
        if(srcIdx.isValid())
            sourceRows.append(srcIdx.row());
    }

    m_resultsModel->scaleBpm(sourceRows, factor);
    updateButtons();
}

void BpmAnalyzerResults::saveToTags()
{
    QList<BpmResult> toSave = m_resultsModel->resultsToSave();
    if(toSave.isEmpty())
        return;

    TrackList tracks;
    tracks.reserve(toSave.size());
    for(auto& result : toSave) {
        result.track.replaceExtraTag(QLatin1String{BpmTagField}, result.analyzedBpm);
        tracks.push_back(result.track);
    }

    m_status->setText(tr("Writing to file tags…"));
    m_saveButton->setEnabled(false);

    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged,
                     this, [this]() {
                         m_resultsModel->markSaved();
                         m_status->setText(tr("Tags saved."));
                         updateButtons();
                     },
                     Qt::SingleShotConnection);

    m_library->writeTrackMetadata(tracks);
}

void BpmAnalyzerResults::updateButtons()
{
    m_saveButton->setEnabled(!m_resultsModel->resultsToSave().isEmpty());
}

void BpmAnalyzerResults::closeEvent(QCloseEvent* event)
{
    if(m_scanning && m_scanner) {
        // Disconnect first so the watcher's queued finished signal can't
        // reach onScanFinished after we null the pointer.
        QObject::disconnect(m_scanner, nullptr, this, nullptr);
        m_scanner->close();
        m_scanner->deleteLater();
        m_scanner = nullptr;
        m_scanning = false;
    }
    QDialog::closeEvent(event);
}

void BpmAnalyzerResults::setupContextMenu()
{
    m_resultsView->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_resultsView, &QTableView::customContextMenuRequested,
                     this, [this](const QPoint& pos) {
                         const QModelIndex proxyIndex = m_resultsView->indexAt(pos);
                         if(!proxyIndex.isValid())
                             return;

                         const QModelIndex srcIndex =
                             m_proxyModel->mapToSource(proxyIndex);
                         const BpmResult result =
                             m_resultsModel->results().at(srcIndex.row());

                         QMenu menu{this};

                         auto* openFolder =
                             menu.addAction(tr("Open Containing Folder"));
                         menu.addSeparator();
                         auto* copyBpm =
                             menu.addAction(tr("Copy Analyzed BPM"));
                         copyBpm->setEnabled(!result.analyzedBpm.isEmpty());
                         auto* copyStored =
                             menu.addAction(tr("Copy Stored BPM"));
                         copyStored->setEnabled(!result.storedBpm.isEmpty());

                         QObject::connect(openFolder, &QAction::triggered, this,
                                          [&result]() {
                                              const QString dir =
                                                  QFileInfo{result.track.filepath()}.absolutePath();
                                              QDesktopServices::openUrl(
                                                  QUrl::fromLocalFile(dir));
                                          });

                         QObject::connect(copyBpm, &QAction::triggered, this,
                                          [&result]() {
                                              QApplication::clipboard()->setText(
                                                  result.analyzedBpm);
                                          });

                         QObject::connect(copyStored, &QAction::triggered, this,
                                          [&result]() {
                                              QApplication::clipboard()->setText(
                                                  result.storedBpm);
                                          });

                         menu.exec(m_resultsView->viewport()->mapToGlobal(pos));
                     });
}

QSize BpmAnalyzerResults::sizeHint() const
{
    QSize size = m_resultsView->sizeHint();
    size.rheight() += 200;
    size.rwidth()  += 500;
    return size;
}

QSize BpmAnalyzerResults::minimumSizeHint() const
{
    return QDialog::minimumSizeHint();
}

} // namespace Fooyin::BpmAnalyzer

#include "moc_bpmanalyzerresults.cpp"
