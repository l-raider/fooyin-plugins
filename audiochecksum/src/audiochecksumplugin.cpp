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

    auto* action = new QAction(tr("Audio Checksum…"), Utils::getMainWindow());
    action->setStatusTip(tr("Calculate or verify MD5 checksums for selected tracks"));
    selectionMenu->menu()->addAction(action);

    const auto updateAction = [this, action]() {
        action->setEnabled(!m_selectionController->selectedTracks().empty());
    };

    QObject::connect(action, &QAction::triggered, this, [this]() {
        const TrackList tracks = m_selectionController->selectedTracks();
        if(tracks.empty())
            return;
        auto* dlg = new AudioChecksumResults(m_library, m_audioLoader, tracks,
                                             Utils::getMainWindow());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    QObject::connect(m_selectionController,
                     &TrackSelectionController::selectionChanged,
                     this, updateAction);

    updateAction();
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumplugin.cpp"
