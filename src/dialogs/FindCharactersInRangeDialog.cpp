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

#include "FindCharactersInRangeDialog.h"

#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>


// Decodes the UTF-8 character starting at pos, returns its length in bytes
static int decodeUtf8(const QByteArray &data, int pos, uint &codePoint)
{
    const unsigned char c = static_cast<unsigned char>(data.at(pos));
    int length = 1;

    if (c < 0x80) { codePoint = c; return 1; }
    else if ((c & 0xE0) == 0xC0) { codePoint = c & 0x1F; length = 2; }
    else if ((c & 0xF0) == 0xE0) { codePoint = c & 0x0F; length = 3; }
    else if ((c & 0xF8) == 0xF0) { codePoint = c & 0x07; length = 4; }
    else { codePoint = c; return 1; } // Invalid byte, treat it as a single character

    if (pos + length > data.size()) {
        codePoint = c;
        return 1;
    }

    for (int i = 1; i < length; ++i) {
        codePoint = (codePoint << 6) | (static_cast<unsigned char>(data.at(pos + i)) & 0x3F);
    }

    return length;
}

FindCharactersInRangeDialog::FindCharactersInRangeDialog(MainWindow *window) :
    QDialog(window, Qt::Dialog),
    window(window)
{
    setWindowTitle(tr("Find Characters in Range"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    radioNonAscii = new QRadioButton(tr("Non-ASCII characters (128-255)"));
    radioAscii = new QRadioButton(tr("ASCII characters (0-127)"));
    radioCustom = new QRadioButton(tr("My range:"));
    radioNonAscii->setChecked(true);

    spinFrom = new QSpinBox();
    spinTo = new QSpinBox();
    spinFrom->setRange(0, 0x10FFFF);
    spinTo->setRange(0, 0x10FFFF);
    spinTo->setValue(255);
    spinFrom->setEnabled(false);
    spinTo->setEnabled(false);
    connect(radioCustom, &QRadioButton::toggled, spinFrom, &QSpinBox::setEnabled);
    connect(radioCustom, &QRadioButton::toggled, spinTo, &QSpinBox::setEnabled);

    QGroupBox *rangeGroup = new QGroupBox(tr("Range"));
    QGridLayout *rangeLayout = new QGridLayout(rangeGroup);
    rangeLayout->addWidget(radioNonAscii, 0, 0, 1, 4);
    rangeLayout->addWidget(radioAscii, 1, 0, 1, 4);
    rangeLayout->addWidget(radioCustom, 2, 0);
    rangeLayout->addWidget(spinFrom, 2, 1);
    rangeLayout->addWidget(new QLabel(QStringLiteral("-")), 2, 2);
    rangeLayout->addWidget(spinTo, 2, 3);

    radioUp = new QRadioButton(tr("Up"));
    radioDown = new QRadioButton(tr("Down"));
    radioDown->setChecked(true);
    checkWrap = new QCheckBox(tr("Wrap around"));
    checkWrap->setChecked(true);

    QGroupBox *directionGroup = new QGroupBox(tr("Direction"));
    QHBoxLayout *directionLayout = new QHBoxLayout(directionGroup);
    directionLayout->addWidget(radioUp);
    directionLayout->addWidget(radioDown);

    QPushButton *buttonFind = new QPushButton(tr("Find"));
    QPushButton *buttonClose = new QPushButton(tr("Close"));
    buttonFind->setDefault(true);
    connect(buttonFind, &QPushButton::clicked, this, &FindCharactersInRangeDialog::find);
    connect(buttonClose, &QPushButton::clicked, this, &FindCharactersInRangeDialog::close);

    QVBoxLayout *buttonLayout = new QVBoxLayout();
    buttonLayout->addWidget(buttonFind);
    buttonLayout->addWidget(buttonClose);
    buttonLayout->addStretch();

    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(rangeGroup);
    leftLayout->addWidget(directionGroup);
    leftLayout->addWidget(checkWrap);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addLayout(leftLayout, 1);
    topLayout->addLayout(buttonLayout);

    labelStatus = new QLabel();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(labelStatus);
}

bool FindCharactersInRangeDialog::inRange(uint codePoint) const
{
    if (radioNonAscii->isChecked()) {
        return codePoint >= 128;
    }
    else if (radioAscii->isChecked()) {
        return codePoint < 128;
    }

    return codePoint >= static_cast<uint>(spinFrom->value()) && codePoint <= static_cast<uint>(spinTo->value());
}

void FindCharactersInRangeDialog::find()
{
    ScintillaNext *editor = window->currentEditor();
    const QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char *>(editor->characterPointer()), editor->textLength());
    const int length = data.size();

    labelStatus->clear();

    auto searchForward = [&](int from, int to) -> bool {
        int pos = from;
        while (pos < to) {
            uint codePoint;
            const int charLength = decodeUtf8(data, pos, codePoint);

            if (inRange(codePoint)) {
                editor->goToRange({pos, pos + charLength});
                return true;
            }

            pos += charLength;
        }
        return false;
    };

    auto searchBackward = [&](int from, int to) -> bool {
        int pos = from;
        while (pos > to) {
            // Move to the start of the previous character
            int start = pos - 1;
            while (start > 0 && (static_cast<unsigned char>(data.at(start)) & 0xC0) == 0x80) {
                start--;
            }

            uint codePoint;
            const int charLength = decodeUtf8(data, start, codePoint);

            if (inRange(codePoint)) {
                editor->goToRange({start, start + charLength});
                return true;
            }

            pos = start;
        }
        return false;
    };

    bool found;
    if (radioDown->isChecked()) {
        const int start = editor->selectionEnd();
        found = searchForward(start, length);

        if (!found && checkWrap->isChecked()) {
            found = searchForward(0, start);
        }
    }
    else {
        const int start = editor->selectionStart();
        found = searchBackward(start, 0);

        if (!found && checkWrap->isChecked()) {
            found = searchBackward(length, start);
        }
    }

    if (!found) {
        labelStatus->setText(tr("<font color=\"red\">Can't find any character in the range.</font>"));
    }
}
