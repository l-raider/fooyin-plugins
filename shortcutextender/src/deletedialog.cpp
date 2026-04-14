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

#include "deletedialog.h"

#include "shortcutextenderdefs.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fooyin::ShortcutExtender {

DeleteDialog::DeleteDialog(const TrackList& tracks, SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
{
    setWindowTitle(tr("Delete File"));
    setModal(true);

    const QString message
        = tracks.size() == 1
              ? tr("Are you sure you want to delete \"%1\"?\n%2")
                    .arg(tracks.front().effectiveTitle(), tracks.front().filepath())
              : tr("Are you sure you want to delete %1 tracks?").arg(tracks.size());

    auto* label = new QLabel(message, this);
    label->setWordWrap(true);

    auto* dontAsk = new QCheckBox(tr("Do not ask again"), this);

    auto* buttons      = new QDialogButtonBox(this);
    auto* deleteButton = buttons->addButton(tr("&Delete"), QDialogButtonBox::DestructiveRole);
    auto* cancelButton = buttons->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
    cancelButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(dontAsk);
    layout->addWidget(buttons);

    QObject::connect(deleteButton, &QPushButton::clicked, this, [this, dontAsk, settings]() {
        if(dontAsk->isChecked()) {
            settings->fileSet(Settings::ConfirmDelete, false);
        }
        accept();
    });
    QObject::connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

} // namespace Fooyin::ShortcutExtender
