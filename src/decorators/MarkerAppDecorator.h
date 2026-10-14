/*
 * This file is part of Notepad Next.
 * Copyright 2025 Justin Dailey
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

#include "ApplicationDecorator.h"

class MarkerAppDecorator : public ApplicationDecorator
{
    Q_OBJECT

public:
    explicit MarkerAppDecorator(NotepadNextApplication *app);

    QColor markerColor(int i) const;
    int markerCount() const;

    void mark(ScintillaNext *editor, int i);
    void clear(ScintillaNext *editor, int i);
    void clearAll(ScintillaNext *editor);

    // All ranges marked with style i (or any style if i is -1), in document order
    QList<QPair<int, int>> markedRanges(ScintillaNext *editor, int i) const;

    // Jump to the next/previous range marked with style i (or any style if i is -1)
    bool jumpToMarked(ScintillaNext *editor, int i, bool forward) const;

    // The text of all ranges marked with style i (or any style if i is -1), one per line
    QString markedText(ScintillaNext *editor, int i) const;
};
