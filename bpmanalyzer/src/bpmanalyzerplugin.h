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

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>

#include <QObject>

#include <memory>

namespace Fooyin {
class ActionManager;
class AudioLoader;
class MusicLibrary;
class TrackSelectionController;
} // namespace Fooyin

namespace Fooyin::BpmAnalyzer {

class BpmAnalyzerPlugin : public QObject,
                          public Plugin,
                          public CorePlugin,
                          public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin" FILE "bpmanalyzer.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    [[nodiscard]] bool hasSettings() const override;
    void showSettings(QWidget* parent) override;

private:
    void setupContextMenu();

    std::shared_ptr<AudioLoader> m_audioLoader;
    MusicLibrary*             m_library{nullptr};
    ActionManager*            m_actionManager{nullptr};
    TrackSelectionController* m_selectionController{nullptr};
};

} // namespace Fooyin::BpmAnalyzer
