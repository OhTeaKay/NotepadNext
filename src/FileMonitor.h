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

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

class ScintillaNext;

// Implements Notepad++'s "View > Monitoring (tail -f)". The file is reloaded automatically
// whenever it changes on disk and the editor scrolls to the end. The editor is read-only while monitored.
class FileMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FileMonitor(QObject *parent = nullptr);

    bool isMonitored(ScintillaNext *editor) const;
    void setMonitored(ScintillaNext *editor, bool monitored);

signals:
    void monitoringChanged(ScintillaNext *editor, bool monitored);

private slots:
    void poll();

private:
    void reloadAndScroll(ScintillaNext *editor);

    struct State {
        QPointer<ScintillaNext> editor;
        bool wasReadOnly;
    };

    QHash<ScintillaNext *, State> monitored;
    QTimer timer;
};
