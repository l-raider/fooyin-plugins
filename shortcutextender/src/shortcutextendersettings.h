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

#include "shortcutextenderdefs.h"

#include <QDialog>

class QCheckBox;
class QRadioButton;
class QTableWidget;

namespace Fooyin {
class SettingsManager;

namespace ShortcutExtender {

class ShortcutExtenderSettings : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutExtenderSettings(SettingsManager* settings, QWidget* parent = nullptr);

    void accept() override;

private:
    void load();
    void reset();
    void applyPresetConfigs();

    SettingsManager* m_settings;

    QCheckBox*    m_confirmDelete;
    QRadioButton* m_trashMode;
    QRadioButton* m_permanentMode;

    QTableWidget* m_presetTable;
    // Preset names in row order (for reading combo values back)
    QStringList m_presetNames;
};

} // namespace ShortcutExtender
} // namespace Fooyin
