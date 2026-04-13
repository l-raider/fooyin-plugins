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

#include "audiochecksumplugin_settings.h"

#include "audiochecksumdefs.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

using namespace Qt::StringLiterals;

namespace Fooyin::AudioChecksum {

AudioChecksumSettingsDialog::AudioChecksumSettingsDialog(QWidget* parent)
    : QDialog{parent}
    , m_tagField{new QLineEdit(this)}
{
    setWindowTitle(tr("Audio Checksum Settings"));
    setModal(true);

    auto* tagLabel = new QLabel(tr("Tag field name") + ":"_L1, this);

    m_tagField->setText(
        m_settings.value(QLatin1String{SettingTagField},
                         QLatin1String{DefaultTagFieldName}).toString());

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     this, &AudioChecksumSettingsDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     this, &AudioChecksumSettingsDialog::reject);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(tagLabel,   row, 0);
    layout->addWidget(m_tagField, row++, 1);
    layout->addWidget(buttons,    row++, 0, 1, 2, Qt::AlignBottom);
}

void AudioChecksumSettingsDialog::accept()
{
    const QString value = m_tagField->text().trimmed().toUpper();
    if(!value.isEmpty())
        m_settings.setValue(QLatin1String{SettingTagField}, value);
    done(Accepted);
}

} // namespace Fooyin::AudioChecksum
