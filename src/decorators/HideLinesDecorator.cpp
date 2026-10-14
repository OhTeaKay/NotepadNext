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

#include "HideLinesDecorator.h"


HideLinesDecorator::HideLinesDecorator(ScintillaNext *editor) :
    EditorDecorator(editor)
{
    editor->markerDefine(MARK_HIDELINESBEGIN, SC_MARK_ARROWDOWN);
    editor->markerDefine(MARK_HIDELINESEND, SC_MARK_ARROW);
    editor->markerDefine(MARK_HIDELINESUNDERLINE, SC_MARK_UNDERLINE);

    for (int marker : {MARK_HIDELINESBEGIN, MARK_HIDELINESEND}) {
        editor->markerSetFore(marker, 0x1F7A1F);
        editor->markerSetBack(marker, 0x77CC77);
    }
    editor->markerSetBack(MARK_HIDELINESUNDERLINE, 0x77CC77);

    const int mask = editor->marginMaskN(MARGIN);
    editor->setMarginMaskN(MARGIN, mask | (1 << MARK_HIDELINESBEGIN) | (1 << MARK_HIDELINESEND) | (1 << MARK_HIDELINESUNDERLINE));
    editor->setMarginSensitiveN(MARGIN, true);
}

bool HideLinesDecorator::lineHasHideMarker(ScintillaNext *editor, int line)
{
    const int markers = editor->markerGet(line);
    return markers & ((1 << MARK_HIDELINESBEGIN) | (1 << MARK_HIDELINESEND));
}

void HideLinesDecorator::hideSelectedLines()
{
    int firstLine = editor->lineFromPosition(editor->selectionStart());
    int lastLine = editor->lineFromPosition(editor->selectionEnd());

    if (lastLine > firstLine && editor->selectionEnd() == editor->positionFromLine(lastLine)) {
        lastLine--;
    }

    // There has to be at least one line left visible
    if (firstLine == 0 && lastLine >= editor->lineCount() - 1) {
        return;
    }

    if (firstLine > 0) {
        editor->markerAdd(firstLine - 1, MARK_HIDELINESBEGIN);
        editor->markerAdd(firstLine - 1, MARK_HIDELINESUNDERLINE);
    }

    if (lastLine < editor->lineCount() - 1) {
        editor->markerAdd(lastLine + 1, MARK_HIDELINESEND);
    }

    // Remove any markers within the block that is now hidden
    for (int line = firstLine; line <= lastLine; ++line) {
        editor->markerDelete(line, MARK_HIDELINESBEGIN);
        editor->markerDelete(line, MARK_HIDELINESEND);
        editor->markerDelete(line, MARK_HIDELINESUNDERLINE);
    }

    editor->hideLines(firstLine, lastLine);

    // Move the caret to a visible line
    const int caretLine = (lastLine < editor->lineCount() - 1) ? lastLine + 1 : firstLine - 1;
    editor->gotoLine(caretLine);
}

void HideLinesDecorator::showAllHiddenLines()
{
    editor->showLines(0, editor->lineCount() - 1);
    editor->markerDeleteAll(MARK_HIDELINESBEGIN);
    editor->markerDeleteAll(MARK_HIDELINESEND);
    editor->markerDeleteAll(MARK_HIDELINESUNDERLINE);
}

void HideLinesDecorator::showHiddenLinesAfter(int line)
{
    int end = line + 1;
    while (end < editor->lineCount() && !editor->lineVisible(end)) {
        end++;
    }

    if (end > line + 1) {
        editor->showLines(line + 1, end - 1);
    }

    editor->markerDelete(line, MARK_HIDELINESBEGIN);
    editor->markerDelete(line, MARK_HIDELINESUNDERLINE);

    if (end < editor->lineCount()) {
        editor->markerDelete(end, MARK_HIDELINESEND);
    }
}

void HideLinesDecorator::showHiddenLinesBefore(int line)
{
    int start = line - 1;
    while (start >= 0 && !editor->lineVisible(start)) {
        start--;
    }

    if (start + 1 <= line - 1) {
        editor->showLines(start + 1, line - 1);
    }

    editor->markerDelete(line, MARK_HIDELINESEND);

    if (start >= 0) {
        editor->markerDelete(start, MARK_HIDELINESBEGIN);
        editor->markerDelete(start, MARK_HIDELINESUNDERLINE);
    }
}

void HideLinesDecorator::notify(const Scintilla::NotificationData *pscn)
{
    if (pscn->nmhdr.code == Scintilla::Notification::MarginClick && pscn->margin == MARGIN) {
        const int line = editor->lineFromPosition(pscn->position);
        const int markers = editor->markerGet(line);

        if (markers & (1 << MARK_HIDELINESBEGIN)) {
            showHiddenLinesAfter(line);
        }
        else if (markers & (1 << MARK_HIDELINESEND)) {
            showHiddenLinesBefore(line);
        }
    }
}
