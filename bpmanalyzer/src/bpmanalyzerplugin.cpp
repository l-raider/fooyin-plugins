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

#include "bpmanalyzerplugin.h"

#include "bpmanalyzerdefs.h"
#include "bpmanalyzerresults.h"
#include "bpmanalyzerplugin_settings.h"

#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

namespace Fooyin::BpmAnalyzer {

void BpmAnalyzerPlugin::initialise(const CorePluginContext& context)
{
    m_audioLoader = context.audioLoader;
    m_library     = context.library;
}

void BpmAnalyzerPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager       = context.actionManager;
    m_selectionController = context.trackSelection;

    setupContextMenu();
}

void BpmAnalyzerPlugin::setupContextMenu()
{
    auto* utilitiesMenu =
        m_actionManager->actionContainer(Constants::Menus::Context::Utilities);

    auto* action = new QAction(tr("BPM Analyzer…"), Utils::getMainWindow());
    action->setStatusTip(tr("Analyze BPM for selected tracks and optionally write to BPM tag"));
    utilitiesMenu->menu()->addAction(action);

    const auto updateAction = [this, action]() {
        action->setEnabled(!m_selectionController->selectedTracks().empty());
    };

    QObject::connect(action, &QAction::triggered, this, [this]() {
        const TrackList tracks = m_selectionController->selectedTracks();
        if(tracks.empty())
            return;
        auto* dlg = new BpmAnalyzerResults(m_library, m_audioLoader, tracks,
                                           Utils::getMainWindow());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    QObject::connect(m_selectionController,
                     &TrackSelectionController::selectionChanged,
                     this, updateAction);

    updateAction();
}

bool BpmAnalyzerPlugin::hasSettings() const
{
    return true;
}

void BpmAnalyzerPlugin::showSettings(QWidget* parent)
{
    auto* dlg = new BpmAnalyzerSettingsDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

} // namespace Fooyin::BpmAnalyzer

#include "moc_bpmanalyzerplugin.cpp"
