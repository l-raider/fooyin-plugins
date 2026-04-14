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

#include "shortcutextenderdefs.h"

#include <utils/settings/settingsmanager.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Fooyin::ShortcutExtender {

ShortcutExtenderSettings::ShortcutExtenderSettings(SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_settings{settings}
    , m_confirmDelete{new QCheckBox(tr("Confirm before deleting tracks"), this)}
    , m_trashMode{new QRadioButton(tr("Move to trash"), this)}
    , m_permanentMode{new QRadioButton(tr("Delete permanently"), this)}
{
    setWindowTitle(tr("Shortcut Extender Settings"));
    setModal(true);

    auto* deleteModeBox    = new QGroupBox(tr("Delete mode"), this);
    auto* deleteModeLayout = new QVBoxLayout(deleteModeBox);
    deleteModeLayout->addWidget(m_trashMode);
    deleteModeLayout->addWidget(m_permanentMode);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &ShortcutExtenderSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &ShortcutExtenderSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
                     &ShortcutExtenderSettings::reset);

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_confirmDelete);
    layout->addWidget(deleteModeBox);
    layout->addWidget(buttons);

    load();
}

void ShortcutExtenderSettings::accept()
{
    m_settings->fileSet(Settings::ConfirmDelete, m_confirmDelete->isChecked());
    m_settings->fileSet(Settings::DeleteMode,
                        static_cast<int>(m_permanentMode->isChecked() ? DeleteMode::Permanent : DeleteMode::Trash));
    done(Accepted);
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
