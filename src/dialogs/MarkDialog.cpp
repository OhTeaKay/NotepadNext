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

#include "MarkDialog.h"

#include "BookMarkDecorator.h"
#include "FindInFilesDialog.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>


MarkDialog::MarkDialog(MainWindow *window) :
    QDialog(window, Qt::Dialog),
    window(window)
{
    setWindowTitle(tr("Mark"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    comboFind = new QComboBox();
    comboFind->setEditable(true);
    comboFind->setInsertPolicy(QComboBox::NoInsert);
    comboFind->setCompleter(nullptr);
    comboFind->setMinimumWidth(300);

    checkBookmarkLine = new QCheckBox(tr("Bookmark line"));
    checkPurge = new QCheckBox(tr("Purge for each search"));
    checkPurge->setChecked(true);
    checkWholeWord = new QCheckBox(tr("Match &whole word only"));
    checkMatchCase = new QCheckBox(tr("Match &case"));
    checkInSelection = new QCheckBox(tr("In select&ion"));

    radioNormal = new QRadioButton(tr("&Normal"));
    radioExtended = new QRadioButton(tr("E&xtended (\\n, \\r, \\t, \\0, \\x...)"));
    radioRegex = new QRadioButton(tr("Re&gular expression"));
    radioNormal->setChecked(true);
    connect(radioRegex, &QRadioButton::toggled, checkWholeWord, [=](bool regex) { checkWholeWord->setEnabled(!regex); });

    QGroupBox *searchMode = new QGroupBox(tr("Search Mode"));
    QVBoxLayout *searchModeLayout = new QVBoxLayout(searchMode);
    searchModeLayout->addWidget(radioNormal);
    searchModeLayout->addWidget(radioExtended);
    searchModeLayout->addWidget(radioRegex);

    QPushButton *buttonMarkAll = new QPushButton(tr("Mark All"));
    QPushButton *buttonClear = new QPushButton(tr("Clear all marks"));
    QPushButton *buttonCopy = new QPushButton(tr("Copy Marked Text"));
    QPushButton *buttonClose = new QPushButton(tr("Close"));
    buttonMarkAll->setDefault(true);

    connect(buttonMarkAll, &QPushButton::clicked, this, &MarkDialog::markAll);
    connect(buttonClear, &QPushButton::clicked, this, &MarkDialog::clearAllMarks);
    connect(buttonCopy, &QPushButton::clicked, this, &MarkDialog::copyMarkedText);
    connect(buttonClose, &QPushButton::clicked, this, &MarkDialog::close);

    QHBoxLayout *findLayout = new QHBoxLayout();
    findLayout->addWidget(new QLabel(tr("Find what:")));
    findLayout->addWidget(comboFind, 1);

    QVBoxLayout *optionsLayout = new QVBoxLayout();
    optionsLayout->addLayout(findLayout);
    optionsLayout->addWidget(checkBookmarkLine);
    optionsLayout->addWidget(checkPurge);
    optionsLayout->addWidget(checkWholeWord);
    optionsLayout->addWidget(checkMatchCase);
    optionsLayout->addWidget(checkInSelection);
    optionsLayout->addWidget(searchMode);

    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    buttonsLayout->addWidget(buttonMarkAll);
    buttonsLayout->addWidget(buttonClear);
    buttonsLayout->addWidget(buttonCopy);
    buttonsLayout->addWidget(buttonClose);
    buttonsLayout->addStretch();

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addLayout(optionsLayout, 1);
    topLayout->addLayout(buttonsLayout);

    labelStatus = new QLabel();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(labelStatus);
}

void MarkDialog::setFindText(const QString &text)
{
    if (!text.isEmpty()) {
        comboFind->setCurrentText(text);
    }

    comboFind->setFocus();
    comboFind->lineEdit()->selectAll();
}

int MarkDialog::markIndicator(ScintillaNext *editor)
{
    const int indicator = editor->allocateIndicator(QStringLiteral("find_mark"));

    // Same look as Notepad++'s "Find Mark Style"
    editor->indicSetStyle(indicator, INDIC_ROUNDBOX);
    editor->indicSetFore(indicator, 0x0000FF); // BGR
    editor->indicSetAlpha(indicator, 100);
    editor->indicSetOutlineAlpha(indicator, 150);
    editor->indicSetUnder(indicator, true);

    return indicator;
}

void MarkDialog::clearMarks(ScintillaNext *editor)
{
    editor->setIndicatorCurrent(markIndicator(editor));
    editor->indicatorClearRange(0, editor->length());
}

QString MarkDialog::markedText(ScintillaNext *editor)
{
    const int indicator = markIndicator(editor);
    const int length = editor->length();
    QStringList parts;
    int pos = 0;

    while (pos < length) {
        const int end = editor->indicatorEnd(indicator, pos);

        if (editor->indicatorValueAt(indicator, pos) != 0) {
            parts.append(QString::fromUtf8(editor->get_text_range(pos, end)));
        }

        if (end <= pos) {
            break;
        }

        pos = end;
    }

    return parts.join(QString::fromUtf8(editor->eolString()));
}

void MarkDialog::markAll()
{
    ScintillaNext *editor = window->currentEditor();
    QString text = comboFind->currentText();

    if (text.isEmpty()) {
        return;
    }

    // Update the history
    {
        const QSignalBlocker blocker(comboFind);
        comboFind->removeItem(comboFind->findText(text));
        comboFind->insertItem(0, text);
        comboFind->setCurrentIndex(0);
    }

    if (radioExtended->isChecked()) {
        text = FindInFilesDialog::convertExtended(text);
    }

    if (checkPurge->isChecked()) {
        clearMarks(editor);
    }

    int flags = 0;
    if (checkMatchCase->isChecked()) flags |= SCFIND_MATCHCASE;
    if (radioRegex->isChecked()) flags |= SCFIND_REGEXP;
    else if (checkWholeWord->isChecked()) flags |= SCFIND_WHOLEWORD;

    Sci_CharacterRange range = {0, static_cast<Sci_PositionCR>(editor->length())};
    if (checkInSelection->isChecked() && !editor->selectionEmpty()) {
        range = {static_cast<Sci_PositionCR>(editor->selectionStart()), static_cast<Sci_PositionCR>(editor->selectionEnd())};
    }

    const int indicator = markIndicator(editor);
    BookMarkDecorator *bookmarks = editor->findChild<BookMarkDecorator *>(QString(), Qt::FindDirectChildrenOnly);
    int count = 0;

    editor->setIndicatorCurrent(indicator);
    editor->setSearchFlags(flags);
    editor->forEachMatchInRange(text.toUtf8(), [&](int start, int end) {
        editor->indicatorFillRange(start, end - start);

        if (checkBookmarkLine->isChecked() && bookmarks) {
            const int line = editor->lineFromPosition(start);
            if (!bookmarks->isBookmarkSet(line)) {
                bookmarks->addBookmark(line);
            }
        }

        count++;
        return end;
    }, range);

    if (count > 0) {
        labelStatus->setText(tr("<font color=\"green\">Mark: %Ln match(es)</font>", "", count));
    }
    else {
        labelStatus->setText(tr("<font color=\"red\">Mark: 0 matches</font>"));
    }
}

void MarkDialog::clearAllMarks()
{
    clearMarks(window->currentEditor());
    labelStatus->setText(tr("All marks have been cleared"));
}

void MarkDialog::copyMarkedText()
{
    const QString text = markedText(window->currentEditor());

    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        labelStatus->setText(tr("Marked text has been copied to the clipboard"));
    }
}
