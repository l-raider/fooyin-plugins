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

#include "shortcutextendersettings.h"

#include "fileopsbridge.h"
#include "shortcutextenderdefs.h"

#include <utils/settings/settingsmanager.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Fooyin::ShortcutExtender {

ShortcutExtenderSettings::ShortcutExtenderSettings(SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_settings{settings}
    , m_confirmDelete{new QCheckBox(tr("Confirm before deleting tracks"), this)}
    , m_trashMode{new QRadioButton(tr("Move to trash"), this)}
    , m_permanentMode{new QRadioButton(tr("Delete permanently"), this)}
    , m_presetTable{new QTableWidget(this)}
{
    setWindowTitle(tr("Shortcut Extender Settings"));
    setModal(true);

    auto* deleteModeBox    = new QGroupBox(tr("Delete mode"), this);
    auto* deleteModeLayout = new QVBoxLayout(deleteModeBox);
    deleteModeLayout->addWidget(m_trashMode);
    deleteModeLayout->addWidget(m_permanentMode);

    // --- FileOps Preset Shortcuts section ---
    auto* presetsBox    = new QGroupBox(tr("FileOps Preset Shortcuts"), this);
    auto* presetsLayout = new QVBoxLayout(presetsBox);

    m_presetTable->setColumnCount(4);
    m_presetTable->setHorizontalHeaderLabels(
        {tr("Preset"), tr("Operation"), tr("Track Source"), tr("Execution Mode")});
    m_presetTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_presetTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_presetTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_presetTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_presetTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_presetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_presetTable->verticalHeader()->setVisible(false);

    const auto presets   = getFileOpsPresets();
    const auto configMap = loadPresetConfigs();

    if(presets.empty()) {
        presetsLayout->addWidget(
            new QLabel(tr("No FileOps presets found.\nCreate presets in the FileOps plugin first."), this));
        m_presetTable->setVisible(false);
    }
    else {
        m_presetTable->setRowCount(static_cast<int>(presets.size()));
        for(int row = 0; row < static_cast<int>(presets.size()); ++row) {
            const FileOpPreset& preset = presets[static_cast<std::size_t>(row)];
            m_presetNames.append(preset.name);

            auto* nameItem = new QTableWidgetItem(preset.name);
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            m_presetTable->setItem(row, 0, nameItem);

            auto* opItem = new QTableWidgetItem(operationDisplayName(preset.op));
            opItem->setFlags(opItem->flags() & ~Qt::ItemIsEditable);
            m_presetTable->setItem(row, 1, opItem);

            const FileOpsPresetConfig cfg = configMap.value(preset.name);

            auto* srcCombo = new QComboBox(this);
            srcCombo->addItem(tr("Selected Tracks"), static_cast<int>(PresetTrackSource::Selected));
            srcCombo->addItem(tr("Currently Playing"), static_cast<int>(PresetTrackSource::Playing));
            srcCombo->setCurrentIndex(static_cast<int>(cfg.trackSource));
            m_presetTable->setCellWidget(row, 2, srcCombo);

            auto* modeCombo = new QComboBox(this);
            modeCombo->addItem(tr("Confirm (preview)"), static_cast<int>(PresetExecMode::Confirm));
            modeCombo->addItem(tr("Run directly"), static_cast<int>(PresetExecMode::Direct));
            modeCombo->setCurrentIndex(static_cast<int>(cfg.execMode));
            m_presetTable->setCellWidget(row, 3, modeCombo);
        }
        presetsLayout->addWidget(m_presetTable);
    }

    auto* noteLabel = new QLabel(
        tr("<i>Note: shortcut actions are registered at startup. Restart fooyin after adding or removing presets.</i>"),
        this);
    noteLabel->setWordWrap(true);
    presetsLayout->addWidget(noteLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &ShortcutExtenderSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &ShortcutExtenderSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
                     &ShortcutExtenderSettings::reset);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_confirmDelete);
    layout->addWidget(deleteModeBox);
    layout->addWidget(presetsBox);
    layout->addWidget(buttons);

    setMinimumWidth(550);
    resize(600, 450);

    load();
}

void ShortcutExtenderSettings::accept()
{
    m_settings->fileSet(Settings::ConfirmDelete, m_confirmDelete->isChecked());
    m_settings->fileSet(Settings::DeleteMode,
                        static_cast<int>(m_permanentMode->isChecked() ? DeleteMode::Permanent : DeleteMode::Trash));
    applyPresetConfigs();
    done(Accepted);
}

void ShortcutExtenderSettings::applyPresetConfigs()
{
    if(!m_presetTable->isVisible() || m_presetNames.isEmpty()) {
        return;
    }

    PresetConfigMap configs;
    for(int row = 0; row < m_presetTable->rowCount(); ++row) {
        if(row >= m_presetNames.size()) {
            break;
        }
        const QString& name = m_presetNames.at(row);

        const auto* srcCombo  = qobject_cast<QComboBox*>(m_presetTable->cellWidget(row, 2));
        const auto* modeCombo = qobject_cast<QComboBox*>(m_presetTable->cellWidget(row, 3));
        if(!srcCombo || !modeCombo) {
            continue;
        }

        FileOpsPresetConfig cfg;
        cfg.trackSource = static_cast<PresetTrackSource>(srcCombo->currentData().toInt());
        cfg.execMode    = static_cast<PresetExecMode>(modeCombo->currentData().toInt());
        configs.insert(name, cfg);
    }

    savePresetConfigs(configs);
}

void ShortcutExtenderSettings::load()
{
    m_confirmDelete->setChecked(m_settings->fileValue(Settings::ConfirmDelete, true).toBool());

    const auto mode = static_cast<DeleteMode>(m_settings->fileValue(Settings::DeleteMode,
                                                                    static_cast<int>(DeleteMode::Trash)).toInt());
    if(mode == DeleteMode::Permanent) {
        m_permanentMode->setChecked(true);
    }
    else {
        m_trashMode->setChecked(true);
    }
}

void ShortcutExtenderSettings::reset()
{
    m_settings->fileRemove(Settings::ConfirmDelete);
    m_settings->fileRemove(Settings::DeleteMode);
    load();
}

} // namespace Fooyin::ShortcutExtender
