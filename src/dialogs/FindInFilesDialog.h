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
#include <QRegularExpression>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class MainWindow;
class ISearchResultsHandler;

// Notepad++'s "Find in Files" (Ctrl+Shift+F): search and replace text in all files of a directory
class FindInFilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindInFilesDialog(MainWindow *window);

    void setFindText(const QString &text);
    void setDirectory(const QString &directory);
    void setSearchResultsHandler(ISearchResultsHandler *handler);

    // Shared helpers so the "Mark" dialog can use the same search options
    static QString convertExtended(const QString &text);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void browseDirectory();
    void findAll();
    void replaceInFiles();

private:
    bool buildRegex(QRegularExpression &regex);
    QStringList collectFiles(bool &cancelled);
    QString expandReplacement(const QRegularExpressionMatch &match, const QString &replacement) const;
    void addToHistory(QComboBox *combo, const QString &settingsKey);
    void loadHistory(QComboBox *combo, const QString &settingsKey);

    MainWindow *window;
    ISearchResultsHandler *searchResultsHandler = nullptr;

    QComboBox *comboFind;
    QComboBox *comboReplace;
    QComboBox *comboFilters;
    QComboBox *comboDirectory;

    QCheckBox *checkFollowCurrentDoc;
    QCheckBox *checkSubFolders;
    QCheckBox *checkHiddenFolders;
    QCheckBox *checkWholeWord;
    QCheckBox *checkMatchCase;

    QRadioButton *radioNormal;
    QRadioButton *radioExtended;
    QRadioButton *radioRegex;
    QCheckBox *checkDotMatchesNewline;

    QLabel *labelStatus;
};
