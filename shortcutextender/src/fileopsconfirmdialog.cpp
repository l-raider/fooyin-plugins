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

#include "fileopsconfirmdialog.h"

#include "fileopsbridge.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Fooyin::ShortcutExtender {

FileOpsConfirmDialog::FileOpsConfirmDialog(const QString& presetName,
                                           const std::vector<FileOpsItem>& operations,
                                           QWidget* parent)
    : QDialog{parent}
    , m_label{new QLabel(this)}
    , m_tree{new QTreeWidget(this)}
{
    setWindowTitle(tr("Confirm File Operation"));
    resize(800, 400);

    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({tr("Operation"), tr("Source"), tr("Destination")});

    // Columns are interactive (user-resizable) and the table is sortable
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSortingEnabled(true);
    m_tree->header()->setSortIndicator(1, Qt::AscendingOrder);
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setUniformRowHeights(true);

    int fileOpCount{0};
    for(const FileOpsItem& item : operations) {
        // Skip internal Create/Remove directory entries — show only file ops
        if(item.op == FileOpsOperation::Create || item.op == FileOpsOperation::Remove) {
            continue;
        }
        ++fileOpCount;
        auto* row = new QTreeWidgetItem(m_tree,
            {operationDisplayName(item.op), item.source, item.destination});
        Q_UNUSED(row)
    }

    // Set column widths to a reasonable starting point
    m_tree->header()->resizeSection(0, 80);
    m_tree->header()->resizeSection(1, 340);

    m_label->setText(
        tr("Preset: <b>%1</b> — %n file operation(s) will be performed:", nullptr, fileOpCount)
            .arg(presetName));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Run"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->addWidget(m_tree);
    layout->addWidget(buttons);
}

} // namespace Fooyin::ShortcutExtender
