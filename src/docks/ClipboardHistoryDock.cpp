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

#include "ClipboardHistoryDock.h"

#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QApplication>
#include <QClipboard>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>


static const int MAX_ENTRIES = 30;
static const int TextRole = Qt::UserRole;

ClipboardHistoryDock::ClipboardHistoryDock(MainWindow *window) :
    QDockWidget(window),
    window(window)
{
    setWindowTitle(tr("Clipboard History"));
    setObjectName(QStringLiteral("ClipboardHistoryDock"));

    list = new QListWidget(this);
    list->setUniformItemSizes(true);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QListWidget::itemActivated, this, &ClipboardHistoryDock::pasteItem);
    connect(list, &QListWidget::itemDoubleClicked, this, &ClipboardHistoryDock::pasteItem);

    connect(list, &QListWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        QListWidgetItem *item = list->itemAt(pos);
        QMenu menu(this);

        if (item) {
            menu.addAction(tr("Paste"), this, [=]() { pasteItem(item); });
            menu.addAction(tr("Copy to Clipboard"), this, [=]() { QApplication::clipboard()->setText(item->data(TextRole).toString()); });
            menu.addAction(tr("Delete Entry"), this, [=]() { delete item; });
            menu.addSeparator();
        }

        menu.addAction(tr("Clear History"), list, &QListWidget::clear);
        menu.exec(list->viewport()->mapToGlobal(pos));
    });

    setWidget(list);

    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardHistoryDock::clipboardChanged);

    // Start with whatever is currently in the clipboard
    clipboardChanged();
}

void ClipboardHistoryDock::clipboardChanged()
{
    const QMimeData *data = QApplication::clipboard()->mimeData();

    if (data && data->hasText()) {
        addText(data->text());
    }
}

void ClipboardHistoryDock::addText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    // Move duplicates to the top
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->data(TextRole).toString() == text) {
            delete list->takeItem(i);
            break;
        }
    }

    QString display = text.simplified();
    if (display.size() > 80) {
        display = display.left(80) + QChar(0x2026);
    }

    QListWidgetItem *item = new QListWidgetItem(display);
    item->setData(TextRole, text);
    item->setToolTip(QStringLiteral("<pre>%1</pre>").arg(text.left(2000).toHtmlEscaped()));
    list->insertItem(0, item);

    while (list->count() > MAX_ENTRIES) {
        delete list->takeItem(list->count() - 1);
    }
}

void ClipboardHistoryDock::pasteItem(QListWidgetItem *item)
{
    ScintillaNext *editor = window->currentEditor();

    if (item == Q_NULLPTR || editor == Q_NULLPTR || editor->readOnly()) {
        return;
    }

    const QByteArray text = item->data(TextRole).toString().toUtf8();

    editor->replaceSel(text.constData());
    editor->grabFocus();
}
