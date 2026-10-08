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

#include <core/track.h>

#include <QDialog>
#include <QHash>
#include <QList>
#include <QSet>

#include <chrono>
#include <functional>
#include <memory>

class QCloseEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

namespace Fooyin {
class AudioLoader;
class MusicLibrary;
} // namespace Fooyin

namespace Fooyin::BpmAnalyzer {

class BpmAnalyzerResultsModel;
class BpmAnalyzerScanner;

/*!
 * Non-modal dialog for analyzing and writing BPM values to selected tracks.
 *
 * Opens with an empty table.  The user clicks "Analyze" to run the scan;
 * results populate the table as each track is processed.
 * Double/Halve BPM buttons operate on selected rows (model only; tags not
 * written until "Save to Tags" is clicked).
 */
class BpmAnalyzerResults : public QDialog
{
    Q_OBJECT

public:
    BpmAnalyzerResults(MusicLibrary* library,
                       std::shared_ptr<AudioLoader> audioLoader,
                       TrackList tracks,
                       QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void startScan();
    void onScanFinished();
    void scaleSelectedBpm(float factor);
    void saveToTags();
    void writeNextTag();
    void finishTagWrite();
    void cancelActive();
    void setupContextMenu();
    void updateButtons();

    MusicLibrary* m_library;
    std::shared_ptr<AudioLoader> m_audioLoader;
    TrackList m_tracks;
    BpmAnalyzerScanner* m_scanner{nullptr};

    QTableView* m_resultsView;
    BpmAnalyzerResultsModel* m_resultsModel;
    QSortFilterProxyModel* m_proxyModel;
    QLabel* m_status;
    QProgressBar* m_progressBar;
    QPushButton* m_analyzeButton;
    QPushButton* m_doubleBpmButton;
    QPushButton* m_halveBpmButton;
    QPushButton* m_saveButton;
    QPushButton* m_cancelButton;
    QPushButton* m_closeButton;

    std::function<void()> m_writeCancel;

    TrackList m_writeQueue;                  // tracks still to write (same order as before)
    qsizetype m_writeIndex{0};
    TrackList m_writeInFlightChunk;          // tracks of the write request in flight
    QString m_writeField;                    // tag field being written
    QHash<QString, QString> m_pathToBpm;     // uniqueFilepath -> value
    QSet<QString> m_savedPaths;
    QSet<QString> m_processedPaths;
    int m_writeTotal{0};
    int m_writeSkipped{0};
    int m_writeSucceeded{0};
    int m_writeFailed{0};
    bool m_writeCancelled{false};

    std::chrono::steady_clock::time_point m_scanStart;
    bool m_scanning{false};
    bool m_saving{false};
};

} // namespace Fooyin::BpmAnalyzer
