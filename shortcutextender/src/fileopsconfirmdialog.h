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

#include "fileopsexecutor.h"

#include <QDialog>

#include <vector>

class QLabel;
class QTreeWidget;

namespace Fooyin::ShortcutExtender {

// Lightweight preview/confirm dialog shown before executing a FileOps preset.
// Displays source → destination pairs so the user can verify the operation
// before committing.
class FileOpsConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    FileOpsConfirmDialog(const QString& presetName, const std::vector<FileOpsItem>& operations,
                         QWidget* parent = nullptr);

private:
    QLabel*      m_label;
    QTreeWidget* m_tree;
};

} // namespace Fooyin::ShortcutExtender
