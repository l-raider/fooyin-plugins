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

#include "audiochecksumplugin.h"

#include "audiochecksumdefs.h"
#include "audiochecksumresults.h"
#include "audiochecksumscanner.h"

#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

void AudioChecksumPlugin::initialise(const CorePluginContext& context)
{
    m_audioLoader = context.audioLoader;
    m_library     = context.library;
}

void AudioChecksumPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager       = context.actionManager;
    m_selectionController = context.trackSelection;

    setupContextMenu();
}

void AudioChecksumPlugin::setupContextMenu()
{
    auto* selectionMenu =
        m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);

    auto* checksumMenu = m_actionManager->createMenu(MenuAudioChecksum);
    checksumMenu->menu()->setTitle(tr("Audio Checksum"));
    selectionMenu->addMenu(checksumMenu);

    auto* window = Utils::getMainWindow();

    auto* calcAction     = new QAction(tr("Calculate Checksums"), window);
    auto* calcSaveAction = new QAction(tr("Calculate and Save to Tags"), window);
    auto* verifyAction   = new QAction(tr("Verify Checksums"), window);

    calcAction->setStatusTip(
        tr("Compute an MD5 checksum from the decoded audio of each selected file"));
    calcSaveAction->setStatusTip(
        tr("Compute a checksum and write it to the %1 tag field").arg(
            QString::fromLatin1(TagFieldName)));
    verifyAction->setStatusTip(
        tr("Compare the computed checksum against the value stored in the %1 tag").arg(
            QString::fromLatin1(TagFieldName)));

    checksumMenu->menu()->addAction(calcAction);
    checksumMenu->menu()->addAction(calcSaveAction);
    checksumMenu->menu()->addSeparator();
    checksumMenu->menu()->addAction(verifyAction);

    QObject::connect(calcAction, &QAction::triggered, this,
                     [this] { runScan(ScanMode::CalculateOnly); });
    QObject::connect(calcSaveAction, &QAction::triggered, this,
                     [this] { runScan(ScanMode::CalculateAndSave); });
    QObject::connect(verifyAction, &QAction::triggered, this,
                     [this] { runScan(ScanMode::VerifyOnly); });

    // Keep menu state in sync with selection
    const auto updateActions = [this, checksumMenu, verifyAction]() {
        const TrackList tracks = m_selectionController->selectedTracks();
        const bool hasSelection = !tracks.empty();

        checksumMenu->menu()->setEnabled(hasSelection);

        const bool anyHasTag = std::any_of(
            tracks.cbegin(), tracks.cend(), [](const Track& t) {
                return !t.extraTag(TagFieldName).isEmpty();
            });
        verifyAction->setEnabled(hasSelection && anyHasTag);
    };

    QObject::connect(m_selectionController,
                     &TrackSelectionController::selectionChanged,
                     this, updateActions);

    updateActions();
}

void AudioChecksumPlugin::runScan(ScanMode mode)
{
    const TrackList tracksToScan = m_selectionController->selectedTracks();
    if(tracksToScan.empty())
        return;

    const auto total = static_cast<int>(tracksToScan.size());

    auto* progress = new ElapsedProgressDialog(
        tr("Scanning tracks…"), tr("Abort"), 0, total + 1,
        Utils::getMainWindow());
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setWindowTitle(tr("Audio Checksum Scan"));
    progress->setShowRemaining(true);
    progress->setValue(0);

    auto* scanner = new AudioChecksumScanner(m_audioLoader, this);

    QObject::connect(
        scanner, &AudioChecksumScanner::scanFinished, this,
        [this, scanner, progress, mode](const QList<ChecksumResult>& results) {
            const auto finishTime = progress->elapsedTime();
            scanner->close();
            progress->deleteLater();

            // For VerifyOnly, filter results to those that were actually verified
            QList<ChecksumResult> displayResults = results;

            if(mode == ScanMode::CalculateAndSave) {
                // Show results; the dialog will have "Save to Tags" pre-selected.
                // The user can review before confirming.
                auto* dlg = new AudioChecksumResults(m_library, std::move(displayResults),
                                                     finishTime, Utils::getMainWindow());
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            }
            else {
                auto* dlg = new AudioChecksumResults(m_library, std::move(displayResults),
                                                     finishTime, Utils::getMainWindow());
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            }
        });

    QObject::connect(
        scanner, &AudioChecksumScanner::scanningTrack, this,
        [scanner, progress](const QString& filepath) {
            if(progress->wasCancelled()) {
                scanner->close();
                progress->deleteLater();
                return;
            }
            progress->setValue(progress->value() + 1);
            progress->setText(tr("Current file") + ":\n"_L1 + filepath);
        });

    scanner->scanTracks(tracksToScan);
    progress->startTimer();
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumplugin.cpp"
