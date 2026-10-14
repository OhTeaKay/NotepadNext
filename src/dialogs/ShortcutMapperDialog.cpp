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

#include "ShortcutMapperDialog.h"

#include "ApplicationSettings.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>


static const char *DEFAULT_SHORTCUTS_PROPERTY = "nn_default_shortcuts";

static QString cleanText(QString text)
{
    text.remove(QLatin1Char('&'));
    if (text.endsWith(QLatin1String("..."))) {
        text.chop(3);
    }
    return text;
}

static void collectCategories(QMenu *menu, const QString &category, QHash<QAction *, QString> &categories)
{
    for (QAction *action : menu->actions()) {
        if (action->menu()) {
            collectCategories(action->menu(), category, categories);
        }
        else if (!action->isSeparator() && !categories.contains(action)) {
            categories.insert(action, category);
        }
    }
}

static QString shortcutsToText(const QList<QKeySequence> &shortcuts)
{
    QStringList parts;
    for (const QKeySequence &shortcut : shortcuts) {
        parts.append(shortcut.toString(QKeySequence::NativeText));
    }
    return parts.join(QStringLiteral(", "));
}

void ShortcutMapperDialog::storeDefaultShortcuts(QWidget *actionOwner)
{
    for (QAction *action : actionOwner->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!action->property(DEFAULT_SHORTCUTS_PROPERTY).isValid()) {
            action->setProperty(DEFAULT_SHORTCUTS_PROPERTY, QVariant::fromValue(action->shortcuts()));
        }
    }
}

ShortcutMapperDialog::ShortcutMapperDialog(QWidget *actionOwner, QMenuBar *menuBar, ApplicationSettings *settings, QWidget *parent) :
    QDialog(parent, Qt::Dialog),
    actionOwner(actionOwner),
    menuBar(menuBar),
    settings(settings)
{
    setWindowTitle(tr("Shortcut Mapper"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(750, 550);

    filterEdit = new QLineEdit();
    filterEdit->setPlaceholderText(tr("Filter"));
    filterEdit->setClearButtonEnabled(true);
    connect(filterEdit, &QLineEdit::textChanged, this, &ShortcutMapperDialog::filter);

    table = new QTableWidget(0, 3);
    table->setHorizontalHeaderLabels({tr("Name"), tr("Shortcut"), tr("Category")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(table, &QTableWidget::itemDoubleClicked, this, &ShortcutMapperDialog::modify);

    QPushButton *buttonModify = new QPushButton(tr("Modify"));
    QPushButton *buttonClear = new QPushButton(tr("Clear"));
    QPushButton *buttonReset = new QPushButton(tr("Reset to Default"));
    QPushButton *buttonClose = new QPushButton(tr("Close"));

    connect(buttonModify, &QPushButton::clicked, this, &ShortcutMapperDialog::modify);
    connect(buttonClear, &QPushButton::clicked, this, &ShortcutMapperDialog::clear);
    connect(buttonReset, &QPushButton::clicked, this, &ShortcutMapperDialog::resetToDefault);
    connect(buttonClose, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addWidget(buttonModify);
    buttons->addWidget(buttonClear);
    buttons->addWidget(buttonReset);
    buttons->addStretch();
    buttons->addWidget(buttonClose);

    QLabel *legend = new QLabel(tr("Shortcuts shown in <font color=\"red\">red</font> are used by more than one command."));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(filterEdit);
    layout->addWidget(table, 1);
    layout->addWidget(legend);
    layout->addLayout(buttons);

    // Determine the category (top level menu) of each action
    for (QAction *menuAction : menuBar->actions()) {
        if (menuAction->menu()) {
            collectCategories(menuAction->menu(), cleanText(menuAction->text()), categories);
        }
    }

    populate();
}

QAction *ShortcutMapperDialog::actionForRow(int row) const
{
    return table->item(row, 0)->data(Qt::UserRole).value<QAction *>();
}

void ShortcutMapperDialog::populate()
{
    table->setSortingEnabled(false);
    table->setRowCount(0);

    QList<QAction *> actions;
    for (QAction *action : actionOwner->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (action->objectName().startsWith(QLatin1String("action")) && !action->text().isEmpty() && !action->menu()) {
            actions.append(action);
        }
    }

    // Sort by category and then by name
    std::sort(actions.begin(), actions.end(), [=](QAction *a, QAction *b) {
        const QString categoryA = categories.value(a, tr("Other"));
        const QString categoryB = categories.value(b, tr("Other"));

        if (categoryA != categoryB) {
            return categoryA.localeAwareCompare(categoryB) < 0;
        }

        return cleanText(a->text()).localeAwareCompare(cleanText(b->text())) < 0;
    });

    for (QAction *action : actions) {
        const int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *name = new QTableWidgetItem(cleanText(action->text()));
        name->setData(Qt::UserRole, QVariant::fromValue(action));
        name->setToolTip(action->objectName().mid(6));

        table->setItem(row, 0, name);
        table->setItem(row, 1, new QTableWidgetItem());
        table->setItem(row, 2, new QTableWidgetItem(categories.value(action, tr("Other"))));

        updateRow(row);
    }

    table->resizeColumnToContents(1);
    table->resizeColumnToContents(2);
}

void ShortcutMapperDialog::updateRow(int row)
{
    QAction *action = actionForRow(row);
    QTableWidgetItem *item = table->item(row, 1);

    item->setText(shortcutsToText(action->shortcuts()));

    bool conflict = false;
    for (const QKeySequence &sequence : action->shortcuts()) {
        if (!conflictingActions(action, sequence).isEmpty()) {
            conflict = true;
        }
    }

    item->setForeground(conflict ? QBrush(Qt::red) : QBrush());
}

QList<QAction *> ShortcutMapperDialog::conflictingActions(QAction *action, const QKeySequence &sequence) const
{
    QList<QAction *> conflicts;

    if (sequence.isEmpty()) {
        return conflicts;
    }

    for (QAction *other : actionOwner->findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (other != action && other->shortcuts().contains(sequence)) {
            conflicts.append(other);
        }
    }

    return conflicts;
}

void ShortcutMapperDialog::filter(const QString &text)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        bool visible = text.isEmpty();

        for (int column = 0; column < table->columnCount() && !visible; ++column) {
            visible = table->item(row, column)->text().contains(text, Qt::CaseInsensitive);
        }

        table->setRowHidden(row, !visible);
    }
}

void ShortcutMapperDialog::applyShortcuts(QAction *action, const QList<QKeySequence> &shortcuts)
{
    action->setShortcuts(shortcuts);

    const QString key = QStringLiteral("Shortcuts/") + action->objectName().mid(6);
    const QList<QKeySequence> defaults = action->property(DEFAULT_SHORTCUTS_PROPERTY).value<QList<QKeySequence>>();

    if (shortcuts == defaults) {
        settings->remove(key);
    }
    else {
        QStringList list;
        for (const QKeySequence &shortcut : shortcuts) {
            list.append(shortcut.toString(QKeySequence::PortableText));
        }

        // An empty list can't be stored reliably, so store an empty string which is ignored when loading
        if (list.isEmpty()) {
            list.append(QString());
        }

        settings->setValue(key, list);
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        updateRow(row);
    }
}

void ShortcutMapperDialog::modify()
{
    const int row = table->currentRow();
    if (row < 0) {
        return;
    }

    QAction *action = actionForRow(row);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Shortcut"));
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    QKeySequenceEdit *edit = new QKeySequenceEdit(action->shortcut());
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Press the new shortcut for <b>%1</b>:").arg(cleanText(action->text()).toHtmlEscaped())));
    layout->addWidget(edit);
    layout->addWidget(buttons);
    edit->setFocus();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // Only use the first key combination (like Notepad++)
    QKeySequence sequence = edit->keySequence();
    if (sequence.count() > 1) {
        sequence = QKeySequence(sequence[0]);
    }

    const QList<QAction *> conflicts = conflictingActions(action, sequence);
    if (!conflicts.isEmpty()) {
        QStringList names;
        for (QAction *conflict : conflicts) {
            names.append(cleanText(conflict->text()));
        }

        const auto reply = QMessageBox::question(this, tr("Shortcut Conflict"),
                                                 tr("%1 is already used by: %2.<br><br>Remove it from the other command(s)?")
                                                     .arg(sequence.toString(QKeySequence::NativeText), names.join(QStringLiteral(", ")).toHtmlEscaped()),
                                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            return;
        }

        if (reply == QMessageBox::Yes) {
            for (QAction *conflict : conflicts) {
                QList<QKeySequence> shortcuts = conflict->shortcuts();
                shortcuts.removeAll(sequence);
                applyShortcuts(conflict, shortcuts);
            }
        }
    }

    applyShortcuts(action, sequence.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{sequence});
}

void ShortcutMapperDialog::clear()
{
    const int row = table->currentRow();

    if (row >= 0) {
        applyShortcuts(actionForRow(row), {});
    }
}

void ShortcutMapperDialog::resetToDefault()
{
    const int row = table->currentRow();

    if (row >= 0) {
        QAction *action = actionForRow(row);
        applyShortcuts(action, action->property(DEFAULT_SHORTCUTS_PROPERTY).value<QList<QKeySequence>>());
    }
}
