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


#ifndef AUTOCOMPLETION_H
#define AUTOCOMPLETION_H

#include "EditorDecorator.h"

#include <QStringList>


class AutoCompletion : public EditorDecorator
{
    Q_OBJECT

public:
    explicit AutoCompletion(ScintillaNext *editor);

public slots:
    void notify(const Scintilla::NotificationData *pscn) override;
    void showAutoCompletion();

    // Shows words from the document starting with the word being typed (Notepad++'s "Word Completion").
    // Any extra words (e.g. language keywords) are included too.
    void showWordCompletion(int minimumCharacters = 1, const QStringList &extraWords = QStringList());

    // Completes file system paths (Notepad++'s "Path Completion")
    void showPathCompletion();
};

#endif // AUTOCOMPLETION_H
