/*
 * Fooyin
 * Copyright © 2026
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "shortcutextenderplugin.h"

#include "deletecurrentlyplaying.h"
#include "shortcutextendersettings.h"

#include <core/plugins/coreplugincontext.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/plugins/pluginsettingsprovider.h>

namespace {
class ShortcutExtenderSettingsProvider : public Fooyin::PluginSettingsProvider
{
public:
    explicit ShortcutExtenderSettingsProvider(Fooyin::SettingsManager* settings)
        : m_settings{settings}
    { }

    void showSettings(QWidget* parent) override
    {
        auto* dialog = new Fooyin::ShortcutExtender::ShortcutExtenderSettings(m_settings, parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }

private:
    Fooyin::SettingsManager* m_settings;
};
} // namespace

namespace Fooyin::ShortcutExtender {

ShortcutExtenderPlugin::ShortcutExtenderPlugin() = default;

void ShortcutExtenderPlugin::initialise(const CorePluginContext& context)
{
    m_library          = context.library;
    m_playerController = context.playerController;
    m_settings         = context.settingsManager;
}

void ShortcutExtenderPlugin::initialise(const GuiPluginContext& context)
{
    m_deleteCurrentlyPlaying = std::make_unique<DeleteCurrentlyPlaying>(
        context.actionManager, m_playerController, m_library, m_settings, this);
}

std::unique_ptr<PluginSettingsProvider> ShortcutExtenderPlugin::settingsProvider() const
{
    return std::make_unique<ShortcutExtenderSettingsProvider>(m_settings);
}

} // namespace Fooyin::ShortcutExtender
