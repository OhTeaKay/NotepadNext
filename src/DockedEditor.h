/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
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


#ifndef DOCKEDEDITOR_H
#define DOCKEDEDITOR_H

#include <QObject>

#include "DockManager.h"
#include "ScintillaNext.h"

class DockedEditor : public QObject
{
    Q_OBJECT

private:
    ads::CDockManager* dockManager = Q_NULLPTR;
    ads::CDockAreaWidget* latestDockArea = Q_NULLPTR;
    ScintillaNext *currentEditor = Q_NULLPTR;

public:
    explicit DockedEditor(QWidget *parent);

    ScintillaNext *getCurrentEditor() const;
    ads::CDockAreaWidget *currentDockArea() const;

    QVector<ScintillaNext *> editors() const;

    void switchToEditor(const ScintillaNext *editor);

    int count() const;

    // Editors in the same dock area ("view") as the given editor, in tab order
    QVector<ScintillaNext *> editorsInSameView(const ScintillaNext *editor) const;

    // Moves the tab of the editor to the index within its view
    void moveEditorToIndex(ScintillaNext *editor, int index);

    // Moves the editor to another view, creating a split view if there is only one (Notepad++'s "Move to Other View")
    void moveEditorToOtherView(ScintillaNext *editor);

    // The currently visible editor in a different view than the given editor, if there is one
    ScintillaNext *editorInOtherView(const ScintillaNext *editor) const;

    // Updates the tab icon (saved/unsaved/read-only)
    void refreshEditorIcon(ScintillaNext *editor);

public slots:
    void addEditor(ScintillaNext *editor);

private slots:
    void dockWidgetCloseRequested();
    void editorRenamed(ScintillaNext *editor);

signals:
    void editorAdded(ScintillaNext *editor);
    void editorCloseRequested(ScintillaNext *editor);
    void editorClosed(ScintillaNext *editor);
    void editorActivated(ScintillaNext *editor);
    void editorOrderChanged();

    void contextMenuRequestedForEditor(ScintillaNext *editor);
    void titleBarDoubleClicked();
};

#endif // DOCKEDEDITOR_H
