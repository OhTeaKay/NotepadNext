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
class QLabel;
class QRadioButton;
class QSpinBox;
class MainWindow;

// Notepad++'s "Search > Find characters in range..."
class FindCharactersInRangeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindCharactersInRangeDialog(MainWindow *window);

private slots:
    void find();

private:
    bool inRange(uint codePoint) const;

    MainWindow *window;

    QRadioButton *radioNonAscii;
    QRadioButton *radioAscii;
    QRadioButton *radioCustom;
    QSpinBox *spinFrom;
    QSpinBox *spinTo;
    QRadioButton *radioUp;
    QRadioButton *radioDown;
    QCheckBox *checkWrap;
    QLabel *labelStatus;
};
