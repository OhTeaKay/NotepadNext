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

#pragma once

#include <QDockWidget>

class MainWindow;
class QListWidget;
class QListWidgetItem;

// Notepad++'s "Edit > Clipboard History" panel. Double clicking an entry pastes it.
class ClipboardHistoryDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ClipboardHistoryDock(MainWindow *window);

private slots:
    void clipboardChanged();
    void pasteItem(QListWidgetItem *item);

private:
    void addText(const QString &text);

    MainWindow *window;
    QListWidget *list;
};
