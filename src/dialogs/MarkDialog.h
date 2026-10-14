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

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class MainWindow;
class ScintillaNext;

// Notepad++'s "Mark" dialog (Search > Mark..., Ctrl+M)
class MarkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarkDialog(MainWindow *window);

    void setFindText(const QString &text);

    // The indicator used for the marks in the editor
    static int markIndicator(ScintillaNext *editor);
    static void clearMarks(ScintillaNext *editor);
    static QString markedText(ScintillaNext *editor);

private slots:
    void markAll();
    void clearAllMarks();
    void copyMarkedText();

private:
    MainWindow *window;

    QComboBox *comboFind;
    QCheckBox *checkBookmarkLine;
    QCheckBox *checkPurge;
    QCheckBox *checkWholeWord;
    QCheckBox *checkMatchCase;
    QCheckBox *checkInSelection;
    QRadioButton *radioNormal;
    QRadioButton *radioExtended;
    QRadioButton *radioRegex;
    QLabel *labelStatus;
};
