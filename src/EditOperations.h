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

#include <functional>

#include <QByteArray>
#include <QString>

class ScintillaNext;

// Text manipulation commands modeled after the ones found in Notepad++
namespace EditOperations {

// Case conversions (Edit > Convert Case to)
QString toProperCase(const QString &text, bool blend);
QString toSentenceCase(const QString &text, bool blend);
QString toInvertedCase(const QString &text);
QString toRandomCase(const QString &text);

// Applies the function to the text of every (non-empty) selection
void transformSelectedText(ScintillaNext *editor, const std::function<QString(const QString &)> &func);

// The lines covered by the main selection, or the entire document if there is no selection
void lineRange(ScintillaNext *editor, int &firstLine, int &lastLine);

// Applies the function to each line (without the line ending) in the line range
void transformLines(ScintillaNext *editor, const std::function<QString(const QString &)> &func);

// Blank operations (Edit > Blank Operations)
void trimTrailingSpace(ScintillaNext *editor);
void trimLeadingSpace(ScintillaNext *editor);
void trimLeadingAndTrailingSpace(ScintillaNext *editor);
void eolToSpace(ScintillaNext *editor, bool trimLines);
void tabsToSpaces(ScintillaNext *editor);
void spacesToTabs(ScintillaNext *editor, bool leadingOnly);

// Line operations
void removeBlankLines(ScintillaNext *editor);
void insertBlankLineAbove(ScintillaNext *editor);
void insertBlankLineBelow(ScintillaNext *editor);
void removeLinesNotMatching(ScintillaNext *editor, const std::function<bool(int line)> &keep);

// Block (stream) comments
bool blockCommentDelimiters(const QString &languageName, QByteArray &start, QByteArray &end);
bool blockComment(ScintillaNext *editor);
bool blockUncomment(ScintillaNext *editor);

// Braces
bool goToMatchingBrace(ScintillaNext *editor, bool select);

// Folding
void foldCurrentLevel(ScintillaNext *editor, bool fold);

// Statistics
int countWords(const QString &text);

}
