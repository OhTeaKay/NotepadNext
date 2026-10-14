/*
 * This file is part of Notepad Next.
 * Copyright 2022 Justin Dailey
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


#include "AutoCompletion.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>


using namespace Scintilla;

AutoCompletion::AutoCompletion(ScintillaNext *editor) :
    EditorDecorator(editor)
{
    editor->autoCSetOrder(SC_ORDER_PERFORMSORT);
    editor->autoCSetMaxHeight(10);
}

void AutoCompletion::notify(const NotificationData *pscn)
{
    if (pscn->nmhdr.code == Notification::CharAdded) {
        if (editor->autoCActive())
            return;

        showAutoCompletion();
    }
}

void AutoCompletion::showAutoCompletion()
{
    // Need a minimum number of characters to trigger auto completion
    showWordCompletion(3);
}

void AutoCompletion::showWordCompletion(int minimumCharacters, const QStringList &extraWords)
{
    int curPos = editor->currentPos();
    int startPos = editor->wordStartPosition(curPos, true);
    int endPos = editor->wordEndPosition(curPos, true);

    if ((curPos - startPos) < minimumCharacters)
        return;

    const QByteArray current_word = editor->get_text_range(startPos, curPos);
    QSet<QByteArray> words;

    if (!current_word.isEmpty()) {
        const QByteArray regex = "\\b" + current_word + "[\\w]*";

        editor->setSearchFlags(SCFIND_REGEXP | SCFIND_MATCHCASE);

        // Don't want to find the word that's currently being typed

        // Find everything before this word
        editor->forEachMatchInRange(regex, [&](int start, int end) {
            words.insert(editor->get_text_range(start, end));
            return end;
        }, { 0, startPos});

        // Find everything after this word
        editor->forEachMatchInRange(regex, [&](int start, int end) {
            words.insert(editor->get_text_range(start, end));
            return end;
        }, { endPos, (Sci_PositionCR)editor->length()});
    }

    for (const QString &extra : extraWords) {
        const QByteArray word = extra.toUtf8();

        if (word.startsWith(current_word) && word != current_word) {
            words.insert(word);
        }
    }

    // A word that is exactly what has been typed is not helpful
    words.remove(current_word);

    if (!words.isEmpty()) {
        editor->autoCSetIgnoreCase(false);
        editor->autoCShow(current_word.length(), words.values().join(' '));
    }
}

void AutoCompletion::showPathCompletion()
{
    const int curPos = editor->currentPos();
    const int line = editor->lineFromPosition(curPos);
    const int lineStart = editor->positionFromLine(line);

    // Walk backwards to find the start of the path
    QString text = QString::fromUtf8(editor->get_text_range(lineStart, curPos));
    int pathStart = text.size();
    while (pathStart > 0) {
        const QChar c = text.at(pathStart - 1);
        if (c.isSpace() || c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('(') || c == QLatin1Char('=')) {
            break;
        }
        pathStart--;
    }

    const QString typed = text.mid(pathStart);
    QString directory;
    QString prefix;

    const int lastSeparator = qMax(typed.lastIndexOf(QLatin1Char('/')), typed.lastIndexOf(QLatin1Char('\\')));
    if (lastSeparator >= 0) {
        directory = typed.left(lastSeparator + 1);
        prefix = typed.mid(lastSeparator + 1);
    }
    else {
        prefix = typed;
    }

    QDir dir;
    if (directory.isEmpty()) {
        dir = editor->isFile() ? editor->getFileInfo().dir() : QDir::home();
    }
    else if (QDir::isRelativePath(directory) && editor->isFile()) {
        dir = QDir(editor->getFileInfo().dir().filePath(directory));
    }
    else {
        dir = QDir(directory);
    }

    if (!dir.exists()) {
        return;
    }

    QStringList entries;
    const QFileInfoList infos = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name | QDir::DirsFirst);
    for (const QFileInfo &info : infos) {
        if (info.fileName().startsWith(prefix, Qt::CaseInsensitive)) {
            entries.append(info.fileName());
        }
    }

    if (entries.isEmpty()) {
        return;
    }

    editor->autoCSetIgnoreCase(true);
    // File names can contain spaces, so use a different separator while the list is created
    editor->autoCSetSeparator('\n');
    editor->autoCShow(prefix.toUtf8().length(), entries.join(QLatin1Char('\n')).toUtf8());
    editor->autoCSetSeparator(' ');
}
