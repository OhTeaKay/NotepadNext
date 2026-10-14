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

#include <QString>
#include <QVector>

// A lightweight parser used by the Function List panel. It does not try to be a compiler,
// it uses the same kind of heuristics as the regular expressions in Notepad++'s function list.
namespace FunctionListParser {

struct Entry {
    QString name;
    QString container; // e.g. the class the function belongs to, empty for top level entries
    int line = 0;      // 0-based line number
    bool isContainer = false; // e.g. a class/struct/heading
    int level = 0;     // nesting level for markdown headings
};

bool supportsLanguage(const QString &languageName);

QVector<Entry> parse(const QString &languageName, const QString &text);

}
