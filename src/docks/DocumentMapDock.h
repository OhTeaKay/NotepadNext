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
class ScintillaEdit;
class QWidget;

// Notepad++'s "Document Map": a zoomed out view of the whole document that highlights
// the visible area of the editor and can be clicked/dragged to scroll the editor.
class DocumentMapDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit DocumentMapDock(MainWindow *window);
    ~DocumentMapDock() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void setEditor(ScintillaNext *editor);
    void syncStyles();
    void updateViewRegion();

private:
    void scrollEditorTo(int y);
    void detachDocument();

    MainWindow *window;
    ScintillaEdit *map;
    QWidget *viewRegion;
    QPointer<ScintillaNext> editor;
    QList<QMetaObject::Connection> editorConnections;
    bool dragging = false;
};
