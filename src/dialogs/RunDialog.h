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
#include <QList>
#include <QPair>

class QComboBox;
class ScintillaNext;
class ApplicationSettings;

// Notepad++'s "Run > Run..." (F5) dialog. Supports the same $(VARIABLES) as Notepad++.
class RunDialog : public QDialog
{
    Q_OBJECT

public:
    RunDialog(ApplicationSettings *settings, ScintillaNext *editor, QWidget *parent = nullptr);

    // Saved commands are stored as (name, command) pairs
    static QList<QPair<QString, QString>> savedCommands(ApplicationSettings *settings);
    static void setSavedCommands(ApplicationSettings *settings, const QList<QPair<QString, QString>> &commands);

    static QString expandVariables(const QString &command, ScintillaNext *editor, bool urlEncode = false);
    static bool execute(const QString &command, ScintillaNext *editor);

signals:
    void savedCommandsChanged();

private slots:
    void run();
    void save();
    void browse();

private:
    void addToHistory(const QString &command);

    ApplicationSettings *settings;
    ScintillaNext *editor;
    QComboBox *comboCommand;
};
