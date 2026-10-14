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
#include <QPointer>

class MainWindow;
class ScintillaNext;
class QLineEdit;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// Notepad++'s "Function List" panel
class FunctionListDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit FunctionListDock(MainWindow *window);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void setEditor(ScintillaNext *editor);
    void refresh();
    void applyFilter(const QString &text);
    void goToItem(QTreeWidgetItem *item);

private:
    MainWindow *window;
    QPointer<ScintillaNext> editor;
    QList<QMetaObject::Connection> editorConnections;

    QLineEdit *filterEdit;
    QToolButton *sortButton;
    QTreeWidget *tree;
    QTimer *refreshTimer;
};
