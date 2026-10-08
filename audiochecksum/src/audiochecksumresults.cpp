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
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTextStream>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

namespace {

// Tracks per write request. Batching keeps per-item progress visible while
// avoiding one whole-library metadata commit in fooyin per saved track.
constexpr qsizetype WriteChunkSize = 16;

// RFC 4180 quoting: wrap when the field contains a delimiter, quote or newline.
QString csvEscape(QString value)
{
    if(!value.contains(u',') && !value.contains(u'"') && !value.contains(u'\n') && !value.contains(u'\r'))
        return value;

    value.replace(u'"', u"\"\""_s);
    value.prepend(u'"');
    value.append(u'"');
    return value;
}

} // namespace

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
    , m_exportButton{new QPushButton(tr("Export to &CSV"), this)}
    , m_saveButton{new QPushButton(tr("&Save to Tags"), this)}
    , m_cancelButton{new QPushButton(tr("Cancel"), this)}
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
    m_exportButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    m_closeButton->setDefault(true);

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);

    QObject::connect(m_calcButton, &QPushButton::clicked, this,
                     [this]() { startScan(); });
    QObject::connect(m_exportButton, &QPushButton::clicked, this,
                     &AudioChecksumResults::exportToCsv);
    QObject::connect(m_saveButton, &QPushButton::clicked, this,
                     &AudioChecksumResults::saveToTags);
    QObject::connect(m_cancelButton, &QPushButton::clicked, this,
                     &AudioChecksumResults::cancelActive);
    QObject::connect(m_closeButton, &QPushButton::clicked, this,
                     &QDialog::close);

    setupContextMenu();

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_calcButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_cancelButton);
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
    if(m_scanning || m_saving)
        return;

    m_scanning = true;
    m_calcButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_cancelButton->setEnabled(true);

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

    QObject::connect(m_scanner, &AudioChecksumScanner::trackScanned, this,
                     [this](const ChecksumResult& result) {
                         m_resultsModel->appendResult(result);
                     });

    QObject::connect(m_scanner, &AudioChecksumScanner::scanFinished, this,
                     &AudioChecksumResults::onScanFinished);

    m_scanner->scanTracks(m_tracks);
}

void AudioChecksumResults::onScanFinished()
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

    m_calcButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    updateSaveButton();
}

void AudioChecksumResults::saveToTags()
{
    if(m_saving)
        return;

    QList<ChecksumResult> toSave = m_resultsModel->resultsToSave();
    if(toSave.isEmpty())
        return;

    m_writeField   = AudioChecksumSettings::load().tagField;
    m_writeSkipped = m_resultsModel->nonWritableCount();

    m_writeQueue.clear();
    m_writeIndex = 0;
    m_savedPaths.clear();
    m_processedPaths.clear();
    m_pathToHash.clear();
    m_writeSucceeded = 0;
    m_writeFailed    = 0;
    m_writeCancelled = false;

    m_writeQueue.reserve(toSave.size());
    for(auto& result : toSave) {
        result.track.replaceExtraTag(m_writeField, result.computedHash);
        const QString trackKey = result.track.uniqueFilepath();
        m_pathToHash.insert(trackKey, result.computedHash);
        m_writeQueue.push_back(result.track);
    }

    m_writeTotal = static_cast<int>(m_pathToHash.size());
    if(m_writeTotal <= 0)
        return;

    m_saving = true;
    m_progressBar->setRange(0, m_writeTotal);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_status->setText(tr("Preparing to write %1 tag(s)…").arg(m_writeTotal));
    m_calcButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_cancelButton->setEnabled(true);

    writeNextTag();
}

void AudioChecksumResults::writeNextTag()
{
    if(m_writeCancelled || m_writeIndex >= static_cast<qsizetype>(m_writeQueue.size())) {
        finishTagWrite();
        return;
    }

    const qsizetype chunkEnd =
        std::min(m_writeIndex + WriteChunkSize, static_cast<qsizetype>(m_writeQueue.size()));

    m_writeInFlightChunk.clear();
    m_writeInFlightChunk.reserve(static_cast<size_t>(chunkEnd - m_writeIndex));
    for(qsizetype i{m_writeIndex}; i < chunkEnd; ++i)
        m_writeInFlightChunk.push_back(m_writeQueue.at(i));
    m_writeIndex = chunkEnd;

    WriteRequest request = m_library->writeTrackMetadata(m_writeInFlightChunk);
    m_writeCancel = request.cancel;

    // Same pattern as fooyin's PropertiesWriteProgress (propertiesdialog.cpp:141):
    // `then` with a context object delivers on the main thread and auto-disconnects
    // if the dialog is destroyed.
    request.finished.then(this, [this](const WriteResult& result) {
        if(result.state == WriteState::Cancelled)
            m_writeCancelled = true;

        m_writeSucceeded += result.succeeded;
        m_writeFailed    += result.failed;

        // The per-request result is authoritative; a chunk that completed with
        // no failures was fully saved even if cancellation was requested while
        // it was in flight.
        const bool chunkSaved = result.failed == 0 && result.state == WriteState::Completed;
        for(const Track& track : m_writeInFlightChunk) {
            const QString path = track.uniqueFilepath();
            m_processedPaths.insert(path);
            if(chunkSaved)
                m_savedPaths.insert(path);
        }

        m_progressBar->setValue(static_cast<int>(m_processedPaths.size()));
        m_status->setText(tr("Writing tags %1 / %2…")
                              .arg(static_cast<int>(m_processedPaths.size()))
                              .arg(m_writeTotal));

        // Queued hop avoids unbounded recursion when the future was already
        // finished (currently-playing / active-source tracks return an
        // immediately-completed request).
        QMetaObject::invokeMethod(this, &AudioChecksumResults::writeNextTag, Qt::QueuedConnection);
    });
}

void AudioChecksumResults::finishTagWrite()
{
    const QString skippedNote = m_writeSkipped > 0
        ? " "_L1 + tr("Skipped %1 non-writable track(s).").arg(m_writeSkipped)
        : QString{};

    // Refresh m_tracks so the next Calculate run reads the stored hash.
    for(Track& track : m_tracks) {
        const QString trackKey = track.uniqueFilepath();
        const auto it = m_pathToHash.constFind(trackKey);
        if(it != m_pathToHash.cend() && m_savedPaths.contains(trackKey))
            track.replaceExtraTag(m_writeField, it.value());
    }

    m_resultsModel->markSaved(m_savedPaths);

    if(m_writeCancelled) {
        m_status->setText(tr("Tag write cancelled (%1 / %2 saved).")
                              .arg(static_cast<int>(m_savedPaths.size()))
                              .arg(m_writeTotal) + skippedNote);
    }
    else if(m_writeFailed == 0) {
        m_progressBar->setValue(m_writeTotal);
        m_status->setText(tr("Tags saved.") + skippedNote);
    }
    else {
        m_status->setText(tr("Saved %1 / %2 tag(s); %3 failed.")
                              .arg(m_writeSucceeded)
                              .arg(m_writeTotal)
                              .arg(m_writeFailed) + skippedNote);
    }

    m_progressBar->setVisible(false);
    m_saving = false;
    m_writeCancel = nullptr;
    m_writeQueue.clear();
    m_cancelButton->setEnabled(false);
    m_calcButton->setEnabled(true);
    updateSaveButton();
}

void AudioChecksumResults::exportToCsv()
{
    const int rows = m_proxyModel->rowCount();
    if(rows == 0)
        return;

    const QString defaultPath =
        QDir{QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)}
            .filePath(u"audiochecksum.csv"_s);

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export to CSV"), defaultPath, tr("CSV files (*.csv);;All files (*)"));
    if(filePath.isEmpty())
        return;

    if(!filePath.endsWith(u".csv"_s, Qt::CaseInsensitive)) {
        filePath += u".csv"_s;

        // The dialog only confirmed overwrite for the name without the suffix;
        // re-confirm for the path that is actually written.
        if(QFileInfo::exists(filePath)
           && QMessageBox::question(
                  this, tr("Export to CSV"),
                  tr("%1 already exists. Do you want to replace it?")
                      .arg(QDir::toNativeSeparators(filePath)),
                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
               != QMessageBox::Yes) {
            return;
        }
    }

    QSaveFile file{filePath};
    if(!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export to CSV"),
                             tr("Could not write %1:\n%2")
                                 .arg(QDir::toNativeSeparators(filePath), file.errorString()));
        return;
    }

    QTextStream out{&file};
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);

    const int columns = m_proxyModel->columnCount();
    QStringList fields;
    fields.reserve(columns);

    for(int col{0}; col < columns; ++col)
        fields.append(csvEscape(m_proxyModel->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString()));
    out << fields.join(u',') << u"\r\n"_s;

    for(int row{0}; row < rows; ++row) {
        fields.clear();
        for(int col{0}; col < columns; ++col)
            fields.append(csvEscape(
                m_proxyModel->data(m_proxyModel->index(row, col), Qt::DisplayRole).toString()));
        out << fields.join(u',') << u"\r\n"_s;
    }
    out.flush();

    if(!file.commit()) {
        QMessageBox::warning(this, tr("Export to CSV"),
                             tr("Could not write %1:\n%2")
                                 .arg(QDir::toNativeSeparators(filePath), file.errorString()));
        return;
    }

    m_status->setText(tr("Exported %1 track(s) to %2")
                          .arg(rows)
                          .arg(QDir::toNativeSeparators(filePath)));
}

void AudioChecksumResults::cancelActive()
{
    if(m_scanning && m_scanner) {
        QObject::disconnect(m_scanner, nullptr, this, nullptr);
        m_scanner->close();
        m_scanner->deleteLater();
        m_scanner = nullptr;
        m_scanning = false;
        m_progressBar->setVisible(false);
        m_status->setText(tr("Scan cancelled."));
        m_calcButton->setEnabled(true);
        m_cancelButton->setEnabled(false);
        updateSaveButton();
    }
    else if(m_saving) {
        m_writeCancelled = true;
        if(m_writeCancel)
            m_writeCancel();
        m_status->setText(tr("Cancelling…"));
        m_cancelButton->setEnabled(false);
    }
}

void AudioChecksumResults::updateSaveButton()
{
    m_saveButton->setEnabled(!m_saving && !m_resultsModel->resultsToSave().isEmpty());
    m_exportButton->setEnabled(!m_scanning && !m_saving && m_resultsModel->rowCount() > 0);
}

void AudioChecksumResults::closeEvent(QCloseEvent* event)
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
    if(m_saving) {
        // The dialog is closing (and will be deleted), so cancel the in-flight
        // tag write instead of losing track of it.
        m_writeCancelled = true;
        if(m_writeCancel)
            m_writeCancel();
        m_writeCancel = nullptr;
        m_saving      = false;
        m_writeQueue.clear();
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
                         const ChecksumResult result =
                             m_resultsModel->results().at(srcIndex.row());

                         const QModelIndexList selectedRows =
                             m_resultsView->selectionModel()->selectedRows();

                         // Right-click inside a multi-selection → copy all selected rows;
                         // otherwise copy only the row under the cursor.
                         QModelIndexList pathRows;
                         if(!selectedRows.isEmpty()
                            && m_resultsView->selectionModel()->isRowSelected(proxyIndex.row(), QModelIndex{})) {
                             pathRows = selectedRows;
                         }
                         else {
                             pathRows.append(proxyIndex.siblingAtColumn(0)); // selectedRows() is column 0
                         }

                         QStringList paths;
                         paths.reserve(pathRows.size());
                         for(const QModelIndex& idx : pathRows) {
                             const QModelIndex src = m_proxyModel->mapToSource(idx);
                             if(!src.isValid())
                                 continue;
                             paths.append(QFileInfo{m_resultsModel->results().at(src.row()).track.filepath()}
                                              .absoluteFilePath());
                         }
                         paths.removeDuplicates(); // CUE chapters share one container file

                         QMenu menu{this};

                         auto* copyPath = menu.addAction(tr("Copy File Path"));
                         copyPath->setEnabled(!paths.isEmpty());
                         menu.addSeparator();

                         auto* openFolder =
                             menu.addAction(tr("Open Containing Folder"));
                         menu.addSeparator();
                         auto* copyComputed =
                             menu.addAction(tr("Copy Computed Checksum"));
                         auto* copyStored =
                             menu.addAction(tr("Copy Stored Checksum"));
                         copyStored->setEnabled(!result.storedHash.isEmpty());

                         QObject::connect(copyPath, &QAction::triggered, this, [paths]() {
                             QApplication::clipboard()->setText(paths.join(u'\n'));
                         });

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

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumresults.cpp"
