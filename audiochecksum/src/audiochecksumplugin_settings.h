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

#pragma once

#include <core/coresettings.h>

#include <QDialog>

class QLineEdit;

namespace Fooyin::AudioChecksum {

class AudioChecksumSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioChecksumSettingsDialog(QWidget* parent = nullptr);

    void accept() override;

private:
    FySettings m_settings;
    QLineEdit* m_tagField;
};

} // namespace Fooyin::AudioChecksum
