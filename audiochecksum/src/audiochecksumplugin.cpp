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
#include "audiochecksumplugin_settings.h"

#include <core/library/musiclibrary.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/plugins/pluginsettingsprovider.h>
#include <gui/trackselectioncontroller.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace {

class AudioChecksumSettingsProvider : public Fooyin::PluginSettingsProvider
{
public:
    void showSettings(QWidget* parent) override
    {
        auto* dlg = new Fooyin::AudioChecksum::AudioChecksumSettingsDialog(parent);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }
};

} // namespace

namespace Fooyin::AudioChecksum {

void AudioChecksumPlugin::initialise(const CorePluginContext& context)
{
    m_audioLoader = context.audioLoader;
    m_library     = context.library;
}

void AudioChecksumPlugin::initialise(const GuiPluginContext& context)
{
    m_selectionController = context.trackSelection;

    setupContextMenu();
}

void AudioChecksumPlugin::setupContextMenu()
{
    auto* action = new QAction(tr("Audio Checksum…"), this);
    action->setStatusTip(tr("Calculate or verify MD5 checksums for selected tracks"));

    QObject::connect(action, &QAction::triggered, this, [this]() {
        const TrackList tracks = m_selectionController->selectedTracks();
        if(tracks.empty())
            return;
        auto* dlg = new AudioChecksumResults(m_library, m_audioLoader, tracks,
                                             Utils::getMainWindow());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    m_selectionController->registerTrackContextAction(
        this, TrackContextMenuArea::Track,
        Constants::Menus::Context::Utilities,
        "AudioChecksum.Calculate",
        tr("Audio Checksum…"),
        [action](QMenu* menu, const TrackSelection& selection) {
            action->setEnabled(!selection.tracks.empty());
            menu->addAction(action);
        });
}

std::unique_ptr<PluginSettingsProvider> AudioChecksumPlugin::settingsProvider() const
{
    return std::make_unique<AudioChecksumSettingsProvider>();
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumplugin.cpp"
