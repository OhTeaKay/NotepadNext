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

#include "EditorDecorator.h"

// Implements Notepad++'s "View > Hide Lines". Hidden blocks are shown with markers in
// the bookmark margin, clicking on a marker shows the lines again.
class HideLinesDecorator : public EditorDecorator
{
    Q_OBJECT

public:
    // These match the markers set up by the EditorManager
    static constexpr int MARK_HIDELINESBEGIN = 23;
    static constexpr int MARK_HIDELINESEND = 22;
    static constexpr int MARK_HIDELINESUNDERLINE = 21;
    static constexpr int MARGIN = 1;

    explicit HideLinesDecorator(ScintillaNext *editor);

    static bool lineHasHideMarker(ScintillaNext *editor, int line);

    void hideSelectedLines();
    void showAllHiddenLines();

public slots:
    void notify(const Scintilla::NotificationData *pscn) override;

private:
    void showHiddenLinesAfter(int line);
    void showHiddenLinesBefore(int line);
};
