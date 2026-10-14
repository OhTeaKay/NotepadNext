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
class QTableWidget;

// Notepad++'s "Edit > Character Panel" (ASCII Codes Insertion Panel). Double click a cell to insert it.
class CharacterPanelDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit CharacterPanelDock(MainWindow *window);

private slots:
    void insertCell(int row, int column);

private:
    void populate();

    MainWindow *window;
    QTableWidget *table;
};
