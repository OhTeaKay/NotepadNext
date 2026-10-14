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

#include "FileMonitor.h"

#include "ScintillaNext.h"


FileMonitor::FileMonitor(QObject *parent) :
    QObject(parent)
{
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, &FileMonitor::poll);
}

bool FileMonitor::isMonitored(ScintillaNext *editor) const
{
    return monitored.contains(editor);
}

void FileMonitor::setMonitored(ScintillaNext *editor, bool enable)
{
    if (enable == isMonitored(editor)) {
        return;
    }

    if (enable) {
        if (!editor->isFile()) {
            return;
        }

        monitored.insert(editor, {editor, editor->readOnly()});

        connect(editor, &ScintillaNext::closed, this, [=]() {
            monitored.remove(editor);
            if (monitored.isEmpty()) {
                timer.stop();
            }
        });

        // Show the latest content right away
        if (editor->isSavedToDisk()) {
            reloadAndScroll(editor);
        }

        editor->setReadOnly(true);
        timer.start();
    }
    else {
        const State state = monitored.take(editor);
        editor->setReadOnly(state.wasReadOnly);
        disconnect(editor, &ScintillaNext::closed, this, nullptr);

        if (monitored.isEmpty()) {
            timer.stop();
        }
    }

    emit monitoringChanged(editor, enable);
}

void FileMonitor::reloadAndScroll(ScintillaNext *editor)
{
    editor->reload();

    editor->documentEnd();
    editor->scrollCaret();
}

void FileMonitor::poll()
{
    const QList<State> states = monitored.values();

    for (const State &state : states) {
        if (!state.editor) {
            continue;
        }

        if (state.editor->checkFileForStateChange() == ScintillaNext::Modified) {
            reloadAndScroll(state.editor);
        }
    }
}
