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
#include <QHash>

class QAction;
class QLineEdit;
class QMenuBar;
class QTableWidget;
class ApplicationSettings;

// Notepad++'s "Settings > Shortcut Mapper...". Custom shortcuts are stored in the [Shortcuts] settings group.
class ShortcutMapperDialog : public QDialog
{
    Q_OBJECT

public:
    ShortcutMapperDialog(QWidget *actionOwner, QMenuBar *menuBar, ApplicationSettings *settings, QWidget *parent = nullptr);

    // Remember the default shortcuts of all actions so they can be restored later
    static void storeDefaultShortcuts(QWidget *actionOwner);

private slots:
    void filter(const QString &text);
    void modify();
    void clear();
    void resetToDefault();

private:
    void populate();
    void updateRow(int row);
    void applyShortcuts(QAction *action, const QList<QKeySequence> &shortcuts);
    QAction *actionForRow(int row) const;
    QList<QAction *> conflictingActions(QAction *action, const QKeySequence &sequence) const;

    QWidget *actionOwner;
    QMenuBar *menuBar;
    ApplicationSettings *settings;

    QLineEdit *filterEdit;
    QTableWidget *table;
    QHash<QAction *, QString> categories;
};
