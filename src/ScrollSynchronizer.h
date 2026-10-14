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

#include <QObject>
#include <QPointer>

class ScintillaNext;

// Implements Notepad++'s "View > Synchronize Vertical/Horizontal Scrolling" between the two views
class ScrollSynchronizer : public QObject
{
    Q_OBJECT

public:
    explicit ScrollSynchronizer(QObject *parent = nullptr);

    void setVertical(bool enabled);
    void setHorizontal(bool enabled);
    bool isEnabled() const { return vertical || horizontal; }

    // Sets the two editors that are kept in sync. The current offset between them is kept.
    void setEditors(ScintillaNext *first, ScintillaNext *second);

private:
    void connectEditors();
    void synchronize(ScintillaNext *source, ScintillaNext *target, int sign);
    void resetOffsets();

    bool vertical = false;
    bool horizontal = false;
    bool syncing = false;

    QPointer<ScintillaNext> first;
    QPointer<ScintillaNext> second;
    QList<QMetaObject::Connection> connections;

    int lineOffset = 0;
    int xOffset = 0;
};
