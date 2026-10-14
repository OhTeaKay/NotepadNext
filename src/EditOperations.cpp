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

#include "EditOperations.h"

#include "ScintillaNext.h"
#include "UndoAction.h"

#include <QHash>
#include <QPair>
#include <QRandomGenerator>
#include <QVector>

#include <algorithm>


namespace EditOperations {

QString toProperCase(const QString &text, bool blend)
{
    QString result = text;
    bool atWordStart = true;

    for (int i = 0; i < result.size(); ++i) {
        const QChar c = result.at(i);

        if (c.isLetter()) {
            if (atWordStart) {
                result[i] = c.toUpper();
            }
            else if (!blend) {
                result[i] = c.toLower();
            }
            atWordStart = false;
        }
        else if (c.isDigit() || c == QLatin1Char('\'') || c == QChar(0x2019)) {
            // Digits and apostrophes are part of a word (e.g. "don't", "3rd")
            atWordStart = false;
        }
        else {
            atWordStart = true;
        }
    }

    return result;
}

QString toSentenceCase(const QString &text, bool blend)
{
    QString result = text;
    bool atSentenceStart = true;

    for (int i = 0; i < result.size(); ++i) {
        const QChar c = result.at(i);

        if (c.isLetter()) {
            if (atSentenceStart) {
                result[i] = c.toUpper();
            }
            else if (!blend) {
                result[i] = c.toLower();
            }
            atSentenceStart = false;
        }
        else if (c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?')) {
            atSentenceStart = true;
        }
        else if (c.isDigit()) {
            atSentenceStart = false;
        }
        else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            // A blank line also starts a new sentence
            if (i + 1 < result.size() && (result.at(i + 1) == QLatin1Char('\n') || result.at(i + 1) == QLatin1Char('\r'))) {
                atSentenceStart = true;
            }
        }
    }

    return result;
}

QString toInvertedCase(const QString &text)
{
    QString result = text;

    for (int i = 0; i < result.size(); ++i) {
        const QChar c = result.at(i);

        if (c.isUpper()) {
            result[i] = c.toLower();
        }
        else if (c.isLower()) {
            result[i] = c.toUpper();
        }
    }

    return result;
}

QString toRandomCase(const QString &text)
{
    QString result = text;

    for (int i = 0; i < result.size(); ++i) {
        const QChar c = result.at(i);

        if (c.isLetter()) {
            result[i] = QRandomGenerator::global()->bounded(2) ? c.toUpper() : c.toLower();
        }
    }

    return result;
}

void transformSelectedText(ScintillaNext *editor, const std::function<QString (const QString &)> &func)
{
    struct Selection {
        int start;
        int end;
        bool caretAtEnd;
        bool isMain;
    };

    QVector<Selection> selections;
    const int mainSelection = editor->mainSelection();

    for (int i = 0; i < editor->selections(); ++i) {
        const int start = editor->selectionNStart(i);
        const int end = editor->selectionNEnd(i);

        if (start < end) {
            selections.append({start, end, editor->selectionNCaret(i) >= editor->selectionNAnchor(i), i == mainSelection});
        }
    }

    if (selections.isEmpty()) {
        return;
    }

    std::sort(selections.begin(), selections.end(), [](const Selection &a, const Selection &b) { return a.start < b.start; });

    const UndoAction ua(editor);
    int delta = 0;

    for (Selection &selection : selections) {
        const int start = selection.start + delta;
        const int end = selection.end + delta;

        const QByteArray original = editor->get_text_range(start, end);
        const QByteArray replacement = func(QString::fromUtf8(original)).toUtf8();

        if (replacement != original) {
            editor->setTargetRange(start, end);
            editor->replaceTarget(replacement.size(), replacement.constData());
        }

        selection.start = start;
        selection.end = start + replacement.size();
        delta += replacement.size() - original.size();
    }

    // Restore the selections
    bool first = true;
    int newMain = 0;
    for (int i = 0; i < selections.size(); ++i) {
        const Selection &s = selections.at(i);
        const int caret = s.caretAtEnd ? s.end : s.start;
        const int anchor = s.caretAtEnd ? s.start : s.end;

        if (first) {
            editor->setSelection(caret, anchor);
            first = false;
        }
        else {
            editor->addSelection(caret, anchor);
        }

        if (s.isMain) {
            newMain = i;
        }
    }

    editor->setMainSelection(newMain);
}

void lineRange(ScintillaNext *editor, int &firstLine, int &lastLine)
{
    if (editor->selectionEmpty()) {
        firstLine = 0;
        lastLine = editor->lineCount() - 1;
        return;
    }

    const int start = editor->selectionStart();
    const int end = editor->selectionEnd();

    firstLine = editor->lineFromPosition(start);
    lastLine = editor->lineFromPosition(end);

    // If the selection ends at the very beginning of a line, don't include it
    if (lastLine > firstLine && end == editor->positionFromLine(lastLine)) {
        lastLine--;
    }
}

void transformLines(ScintillaNext *editor, const std::function<QString (const QString &)> &func)
{
    int firstLine;
    int lastLine;
    lineRange(editor, firstLine, lastLine);

    const UndoAction ua(editor);

    for (int line = lastLine; line >= firstLine; --line) {
        const int start = editor->positionFromLine(line);
        const int end = editor->lineEndPosition(line);

        const QByteArray original = editor->get_text_range(start, end);
        const QByteArray replacement = func(QString::fromUtf8(original)).toUtf8();

        if (replacement != original) {
            editor->setTargetRange(start, end);
            editor->replaceTarget(replacement.size(), replacement.constData());
        }
    }
}

static bool isBlank(QChar c)
{
    return c == QLatin1Char(' ') || c == QLatin1Char('\t');
}

static QString trimRight(const QString &s)
{
    int end = s.size();
    while (end > 0 && isBlank(s.at(end - 1))) {
        --end;
    }
    return s.left(end);
}

static QString trimLeft(const QString &s)
{
    int start = 0;
    while (start < s.size() && isBlank(s.at(start))) {
        ++start;
    }
    return s.mid(start);
}

void trimTrailingSpace(ScintillaNext *editor)
{
    transformLines(editor, trimRight);
}

void trimLeadingSpace(ScintillaNext *editor)
{
    transformLines(editor, trimLeft);
}

void trimLeadingAndTrailingSpace(ScintillaNext *editor)
{
    transformLines(editor, [](const QString &line) { return trimLeft(trimRight(line)); });
}

void eolToSpace(ScintillaNext *editor, bool trimLines)
{
    int firstLine;
    int lastLine;
    lineRange(editor, firstLine, lastLine);

    QStringList lines;
    for (int line = firstLine; line <= lastLine; ++line) {
        QString text = QString::fromUtf8(editor->get_text_range(editor->positionFromLine(line), editor->lineEndPosition(line)));

        if (trimLines) {
            text = trimLeft(trimRight(text));

            // Skip blank lines entirely so there aren't multiple spaces
            if (text.isEmpty()) {
                continue;
            }
        }

        lines.append(text);
    }

    const int start = editor->positionFromLine(firstLine);
    const int end = editor->lineEndPosition(lastLine);
    const QByteArray replacement = lines.join(QLatin1Char(' ')).toUtf8();

    const UndoAction ua(editor);
    editor->setTargetRange(start, end);
    editor->replaceTarget(replacement.size(), replacement.constData());
}

void tabsToSpaces(ScintillaNext *editor)
{
    const int tabWidth = qMax(1, static_cast<int>(editor->tabWidth()));

    transformLines(editor, [tabWidth](const QString &line) {
        if (!line.contains(QLatin1Char('\t'))) {
            return line;
        }

        QString result;
        int column = 0;

        for (const QChar c : line) {
            if (c == QLatin1Char('\t')) {
                const int spaces = tabWidth - (column % tabWidth);
                result.append(QString(spaces, QLatin1Char(' ')));
                column += spaces;
            }
            else {
                result.append(c);
                if (!c.isLowSurrogate()) {
                    column++;
                }
            }
        }

        return result;
    });
}

void spacesToTabs(ScintillaNext *editor, bool leadingOnly)
{
    const int tabWidth = qMax(1, static_cast<int>(editor->tabWidth()));

    transformLines(editor, [tabWidth, leadingOnly](const QString &line) {
        if (!line.contains(QLatin1Char(' '))) {
            return line;
        }

        if (leadingOnly) {
            int column = 0;
            int i = 0;

            for (; i < line.size(); ++i) {
                const QChar c = line.at(i);
                if (c == QLatin1Char(' ')) {
                    column++;
                }
                else if (c == QLatin1Char('\t')) {
                    column += tabWidth - (column % tabWidth);
                }
                else {
                    break;
                }
            }

            return QString(column / tabWidth, QLatin1Char('\t')) + QString(column % tabWidth, QLatin1Char(' ')) + line.mid(i);
        }

        QString result;
        int column = 0;
        int pendingSpaces = 0;
        bool leading = true;

        for (const QChar c : line) {
            if (c == QLatin1Char(' ')) {
                pendingSpaces++;
                column++;

                if (column % tabWidth == 0) {
                    // Spaces that end on a tab stop can be replaced by a tab
                    if (pendingSpaces > 1 || leading) {
                        result.append(QLatin1Char('\t'));
                    }
                    else {
                        result.append(QLatin1Char(' '));
                    }
                    pendingSpaces = 0;
                }
            }
            else if (c == QLatin1Char('\t')) {
                // Any spaces before a tab are absorbed by it
                pendingSpaces = 0;
                result.append(c);
                column += tabWidth - (column % tabWidth);
            }
            else {
                result.append(QString(pendingSpaces, QLatin1Char(' ')));
                pendingSpaces = 0;
                result.append(c);
                leading = false;

                if (!c.isLowSurrogate()) {
                    column++;
                }
            }
        }

        result.append(QString(pendingSpaces, QLatin1Char(' ')));

        return result;
    });
}

static void deleteWholeLine(ScintillaNext *editor, int line)
{
    if (line < editor->lineCount() - 1) {
        const int start = editor->positionFromLine(line);
        editor->deleteRange(start, editor->positionFromLine(line + 1) - start);
    }
    else if (line > 0) {
        // Last line, so remove the line ending of the previous line
        const int start = editor->lineEndPosition(line - 1);
        editor->deleteRange(start, editor->length() - start);
    }
    else {
        editor->deleteRange(0, editor->length());
    }
}

void removeBlankLines(ScintillaNext *editor)
{
    int firstLine;
    int lastLine;
    lineRange(editor, firstLine, lastLine);

    const UndoAction ua(editor);

    for (int line = lastLine; line >= firstLine; --line) {
        const QString text = QString::fromUtf8(editor->get_text_range(editor->positionFromLine(line), editor->lineEndPosition(line)));

        if (text.trimmed().isEmpty() && editor->lineCount() > 1) {
            deleteWholeLine(editor, line);
        }
    }
}

void removeLinesNotMatching(ScintillaNext *editor, const std::function<bool (int)> &keep)
{
    const UndoAction ua(editor);

    for (int line = editor->lineCount() - 1; line >= 0; --line) {
        if (!keep(line)) {
            deleteWholeLine(editor, line);
        }
    }
}

void insertBlankLineAbove(ScintillaNext *editor)
{
    const int line = editor->lineFromPosition(editor->currentPos());
    const int start = editor->positionFromLine(line);
    const QByteArray eol = editor->eolString();

    const UndoAction ua(editor);
    editor->insertText(start, eol.constData());
    editor->gotoPos(start);
}

void insertBlankLineBelow(ScintillaNext *editor)
{
    const int line = editor->lineFromPosition(editor->currentPos());
    const int end = editor->lineEndPosition(line);
    const QByteArray eol = editor->eolString();

    const UndoAction ua(editor);
    editor->insertText(end, eol.constData());
    editor->gotoPos(end + eol.size());
}

bool blockCommentDelimiters(const QString &languageName, QByteArray &start, QByteArray &end)
{
    static const QHash<QString, QPair<QByteArray, QByteArray>> delimiters = {
        {QStringLiteral("ActionScript"), {"/*", "*/"}},
        {QStringLiteral("BaanC"), {"/*", "*/"}},
        {QStringLiteral("C"), {"/*", "*/"}},
        {QStringLiteral("C++"), {"/*", "*/"}},
        {QStringLiteral("C#"), {"/*", "*/"}},
        {QStringLiteral("CSS"), {"/*", "*/"}},
        {QStringLiteral("SCSS"), {"/*", "*/"}},
        {QStringLiteral("Less"), {"/*", "*/"}},
        {QStringLiteral("Csound"), {"/*", "*/"}},
        {QStringLiteral("D"), {"/*", "*/"}},
        {QStringLiteral("Go"), {"/*", "*/"}},
        {QStringLiteral("Java"), {"/*", "*/"}},
        {QStringLiteral("JavaScript"), {"/*", "*/"}},
        {QStringLiteral("JSON"), {"/*", "*/"}},
        {QStringLiteral("NSIS"), {"/*", "*/"}},
        {QStringLiteral("Nix"), {"/*", "*/"}},
        {QStringLiteral("Objective-C"), {"/*", "*/"}},
        {QStringLiteral("PHP"), {"/*", "*/"}},
        {QStringLiteral("RC"), {"/*", "*/"}},
        {QStringLiteral("Rust"), {"/*", "*/"}},
        {QStringLiteral("SQL"), {"/*", "*/"}},
        {QStringLiteral("Swift"), {"/*", "*/"}},
        {QStringLiteral("Verilog"), {"/*", "*/"}},
        {QStringLiteral("VHDL"), {"/*", "*/"}},
        {QStringLiteral("Visual Prolog"), {"/*", "*/"}},
        {QStringLiteral("HTML"), {"<!--", "-->"}},
        {QStringLiteral("XML"), {"<!--", "-->"}},
        {QStringLiteral("asp"), {"<!--", "-->"}},
        {QStringLiteral("Markdown"), {"<!--", "-->"}},
        {QStringLiteral("Lua"), {"--[[", "]]"}},
        {QStringLiteral("Pascal"), {"{", "}"}},
        {QStringLiteral("InnoSetup"), {"{", "}"}},
        {QStringLiteral("Haskell"), {"{-", "-}"}},
        {QStringLiteral("Caml"), {"(*", "*)"}},
        {QStringLiteral("PowerShell"), {"<#", "#>"}},
        {QStringLiteral("LISP"), {"#|", "|#"}},
        {QStringLiteral("Scheme"), {"#|", "|#"}},
        {QStringLiteral("CMakeFile"), {"#[[", "]]"}},
        {QStringLiteral("Nimrod"), {"#[", "]#"}},
        {QStringLiteral("Smalltalk"), {"\"", "\""}},
    };

    const auto it = delimiters.constFind(languageName);
    if (it == delimiters.constEnd()) {
        return false;
    }

    start = it->first;
    end = it->second;

    return true;
}

bool blockComment(ScintillaNext *editor)
{
    QByteArray commentStart;
    QByteArray commentEnd;

    if (!blockCommentDelimiters(editor->languageName, commentStart, commentEnd)) {
        return false;
    }

    int start = editor->selectionStart();
    int end = editor->selectionEnd();

    if (start == end) {
        // Comment the current line (without leading whitespace)
        const int line = editor->lineFromPosition(start);
        start = editor->lineIndentPosition(line);
        end = editor->lineEndPosition(line);

        if (start == end) {
            return false;
        }
    }

    const UndoAction ua(editor);

    editor->insertText(end, commentEnd.constData());
    editor->insertText(start, commentStart.constData());
    editor->setSelection(end + commentStart.size() + commentEnd.size(), start);

    return true;
}

bool blockUncomment(ScintillaNext *editor)
{
    QByteArray commentStart;
    QByteArray commentEnd;

    if (!blockCommentDelimiters(editor->languageName, commentStart, commentEnd) || commentStart == commentEnd) {
        return false;
    }

    const QByteArray text = QByteArray::fromRawData(reinterpret_cast<const char *>(editor->characterPointer()), editor->textLength());

    int startPos = -1;
    int endPos = -1;

    if (!editor->selectionEmpty()) {
        // See if the selection is surrounded by the delimiters (ignoring whitespace)
        int s = editor->selectionStart();
        int e = editor->selectionEnd();

        while (s < e && QChar::isSpace(static_cast<unsigned char>(text.at(s)))) s++;
        while (e > s && QChar::isSpace(static_cast<unsigned char>(text.at(e - 1)))) e--;

        if (e - s >= commentStart.size() + commentEnd.size() &&
            QByteArrayView(text).sliced(s, commentStart.size()) == commentStart &&
            QByteArrayView(text).sliced(e - commentEnd.size(), commentEnd.size()) == commentEnd) {
            startPos = s;
            endPos = e - commentEnd.size();
        }
    }

    if (startPos == -1) {
        // Look for a comment surrounding the caret
        const int caret = editor->currentPos();

        // Search backwards from a position that allows the caret to be inside the opening delimiter
        const int searchFrom = qMin(static_cast<int>(text.size()) - 1, caret + static_cast<int>(commentStart.size()) - 1);
        startPos = text.lastIndexOf(commentStart, searchFrom);

        if (startPos == -1) {
            return false;
        }

        // Make sure the comment has not been closed between the start and the caret
        const int closed = text.indexOf(commentEnd, startPos + commentStart.size());
        if (closed == -1) {
            return false;
        }

        if (closed + commentEnd.size() < caret) {
            return false;
        }

        endPos = closed;
    }

    const UndoAction ua(editor);

    editor->deleteRange(endPos, commentEnd.size());
    editor->deleteRange(startPos, commentStart.size());

    return true;
}

static bool isBrace(char c)
{
    return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '<' || c == '>';
}

bool goToMatchingBrace(ScintillaNext *editor, bool select)
{
    const int caret = editor->currentPos();
    int bracePos = -1;
    bool braceBeforeCaret = false;

    if (caret > 0 && isBrace(static_cast<char>(editor->charAt(caret - 1)))) {
        bracePos = caret - 1;
        braceBeforeCaret = true;
    }
    else if (caret < editor->length() && isBrace(static_cast<char>(editor->charAt(caret)))) {
        bracePos = caret;
    }

    if (bracePos == -1) {
        return false;
    }

    const int match = editor->braceMatch(bracePos, 0);
    if (match == -1) {
        return false;
    }

    if (select) {
        const int start = qMin(bracePos, match);
        const int end = qMax(bracePos, match) + 1;
        editor->setSelection(end, start);
    }
    else {
        const int newPos = braceBeforeCaret ? match + 1 : match;
        editor->gotoPos(newPos);
    }

    editor->scrollCaret();

    return true;
}

void foldCurrentLevel(ScintillaNext *editor, bool fold)
{
    const int line = editor->lineFromPosition(editor->currentPos());
    int header = line;

    if (!(editor->foldLevel(line) & SC_FOLDLEVELHEADERFLAG)) {
        header = editor->foldParent(line);
    }

    if (header < 0) {
        return;
    }

    editor->foldLine(header, fold ? SC_FOLDACTION_CONTRACT : SC_FOLDACTION_EXPAND);

    if (fold) {
        editor->gotoLine(header);
    }
}

int countWords(const QString &text)
{
    int words = 0;
    bool inWord = false;

    for (const QChar c : text) {
        const bool wordChar = c.isLetterOrNumber() || c == QLatin1Char('_') || c.isMark();

        if (wordChar && !inWord) {
            words++;
        }

        inWord = wordChar;
    }

    return words;
}

}
