/*
 * This file is part of Notepad Next.
 * Copyright 2026 Notepad Next contributors
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "WindowsDialog.h"

#include "DockedEditor.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>


// Sorts numerically using the UserRole data
class NumericTableItem : public QTableWidgetItem
{
public:
    using QTableWidgetItem::QTableWidgetItem;

    bool operator<(const QTableWidgetItem &other) const override
    {
        return data(Qt::UserRole + 1).toLongLong() < other.data(Qt::UserRole + 1).toLongLong();
    }
};

WindowsDialog::WindowsDialog(MainWindow *window) :
    QDialog(window, Qt::Dialog),
    window(window)
{
    setWindowTitle(tr("Windows"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(800, 450);

    table = new QTableWidget(0, 4);
    table->setHorizontalHeaderLabels({tr("Name"), tr("Path"), tr("Type"), tr("Size")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->setSortingEnabled(true);
    connect(table, &QTableWidget::itemDoubleClicked, this, &WindowsDialog::activate);

    QPushButton *buttonActivate = new QPushButton(tr("&Activate"));
    QPushButton *buttonSave = new QPushButton(tr("&Save"));
    QPushButton *buttonClose = new QPushButton(tr("&Close window(s)"));
    QPushButton *buttonSort = new QPushButton(tr("Sort &tabs"));
    QPushButton *buttonOk = new QPushButton(tr("&OK"));

    connect(buttonActivate, &QPushButton::clicked, this, &WindowsDialog::activate);
    connect(buttonSave, &QPushButton::clicked, this, &WindowsDialog::saveSelected);
    connect(buttonClose, &QPushButton::clicked, this, &WindowsDialog::closeSelected);
    connect(buttonSort, &QPushButton::clicked, this, &WindowsDialog::sortTabs);
    connect(buttonOk, &QPushButton::clicked, this, &QDialog::accept);

    QVBoxLayout *buttons = new QVBoxLayout();
    buttons->addWidget(buttonActivate);
    buttons->addWidget(buttonSave);
    buttons->addWidget(buttonClose);
    buttons->addWidget(buttonSort);
    buttons->addStretch();
    buttons->addWidget(buttonOk);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(table, 1);
    layout->addLayout(buttons);

    populate();
}

void WindowsDialog::populate()
{
    table->setSortingEnabled(false);
    table->setRowCount(0);

    ScintillaNext *current = window->currentEditor();

    for (ScintillaNext *editor : window->editors()) {
        const int row = table->rowCount();
        table->insertRow(row);

        QString name = editor->getName();
        if (!editor->isSavedToDisk()) {
            name += QStringLiteral("*");
        }

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, QVariant::fromValue(QPointer<ScintillaNext>(editor)));

        QTableWidgetItem *pathItem = new QTableWidgetItem(editor->isFile() ? QDir::toNativeSeparators(editor->getPath()) : QString());
        QTableWidgetItem *typeItem = new QTableWidgetItem(editor->languageName);

        NumericTableItem *sizeItem = new NumericTableItem(QLocale().toString(static_cast<qlonglong>(editor->length())));
        sizeItem->setData(Qt::UserRole + 1, static_cast<qlonglong>(editor->length()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        table->setItem(row, 0, nameItem);
        table->setItem(row, 1, pathItem);
        table->setItem(row, 2, typeItem);
        table->setItem(row, 3, sizeItem);

        if (editor == current) {
            table->selectRow(row);
        }
    }

    table->resizeColumnToContents(0);
    table->resizeColumnToContents(2);

    // Show the documents in tab order until the user sorts them
    table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    table->setSortingEnabled(true);
}

QList<ScintillaNext *> WindowsDialog::selectedEditors() const
{
    QList<ScintillaNext *> editors;

    for (const QModelIndex &index : table->selectionModel()->selectedRows(0)) {
        QPointer<ScintillaNext> editor = table->item(index.row(), 0)->data(Qt::UserRole).value<QPointer<ScintillaNext>>();

        if (editor) {
            editors.append(editor);
        }
    }

    return editors;
}

void WindowsDialog::activate()
{
    const QList<ScintillaNext *> editors = selectedEditors();

    if (!editors.isEmpty()) {
        window->switchToEditor(editors.first());
        accept();
    }
}

void WindowsDialog::saveSelected()
{
    for (ScintillaNext *editor : selectedEditors()) {
        window->saveFile(editor);
    }

    populate();
}

void WindowsDialog::closeSelected()
{
    for (ScintillaNext *editor : selectedEditors()) {
        window->closeFile(editor);
    }

    populate();
}

void WindowsDialog::sortTabs()
{
    // Apply the order shown in the table to the tabs of each view
    ScintillaNext *current = window->currentEditor();
    QList<ScintillaNext *> ordered;
    for (int row = 0; row < table->rowCount(); ++row) {
        QPointer<ScintillaNext> editor = table->item(row, 0)->data(Qt::UserRole).value<QPointer<ScintillaNext>>();

        if (editor) {
            ordered.append(editor);
        }
    }

    DockedEditor *dockedEditor = window->getDockedEditor();
    QSet<ScintillaNext *> handled;

    for (ScintillaNext *editor : ordered) {
        if (handled.contains(editor)) {
            continue;
        }

        // All editors in the same view as this one, in the new order
        const QVector<ScintillaNext *> view = dockedEditor->editorsInSameView(editor);
        int index = 0;

        for (ScintillaNext *e : ordered) {
            if (view.contains(e)) {
                dockedEditor->moveEditorToIndex(e, index++);
                handled.insert(e);
            }
        }
    }

    window->switchToEditor(current);
}
