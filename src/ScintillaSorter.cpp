/*
 * This file is part of Notepad Next.
 * Copyright 2026 Justin Dailey
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

#include "ScintillaSorter.h"
#include "ByteArrayUtils.h"

ScintillaSorter::ScintillaSorter(ScintillaNext *editor)
    : editor(editor)
{
}

void ScintillaSorter::sort(const Sorter &sorter)
{
    // Like Notepad++, if multiple lines are selected only those lines are sorted, else the entire document
    int startPos = 0;
    int endPos = editor->length();
    bool usingSelection = false;

    if (!editor->selectionEmpty()) {
        int firstLine = editor->lineFromPosition(editor->selectionStart());
        int lastLine = editor->lineFromPosition(editor->selectionEnd());

        if (lastLine > firstLine && editor->selectionEnd() == editor->positionFromLine(lastLine)) {
            lastLine--;
        }

        if (lastLine > firstLine) {
            startPos = editor->positionFromLine(firstLine);
            endPos = editor->lineEndPosition(lastLine);
            usingSelection = true;
        }
    }

    const QByteArray eol = editor->eolString();
    const QByteArray text = readEditorText(startPos, endPos);
    QVector<QByteArrayView> lines = ByteArrayUtils::split(text, eol);

    sorter.sort(lines);

    QByteArray result = ByteArrayUtils::join(lines, eol);
    writeEditorText(startPos, endPos, result);

    if (usingSelection) {
        editor->setSelection(startPos + result.size(), startPos);
    }
}

const QByteArray ScintillaSorter::readEditorText(int start, int end)
{
    sptr_t pointer = editor->characterPointer();

    return QByteArray::fromRawData(reinterpret_cast<const char *>(pointer) + start, end - start);
}

void ScintillaSorter::writeEditorText(int start, int end, const QByteArray &result)
{
    editor->setTargetRange(start, end);
    editor->replaceTarget(result.size(), result.constData());
}
