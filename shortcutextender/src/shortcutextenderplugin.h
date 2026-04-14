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

#pragma once

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <gui/plugins/pluginconfigguiplugin.h>

#include <QObject>

#include <memory>

namespace Fooyin {
class MusicLibrary;
class PlayerController;
class SettingsManager;

namespace ShortcutExtender {
class DeleteCurrentlyPlaying;

class ShortcutExtenderPlugin : public QObject,
                               public Plugin,
                               public CorePlugin,
                               public GuiPlugin,
                               public PluginConfigGuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "shortcutextender.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin Fooyin::PluginConfigGuiPlugin)

public:
    ShortcutExtenderPlugin();

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    [[nodiscard]] std::unique_ptr<PluginSettingsProvider> settingsProvider() const override;

private:
    MusicLibrary*    m_library{nullptr};
    PlayerController* m_playerController{nullptr};
    SettingsManager*  m_settings{nullptr};

    std::unique_ptr<DeleteCurrentlyPlaying> m_deleteCurrentlyPlaying;
};

} // namespace ShortcutExtender
} // namespace Fooyin
