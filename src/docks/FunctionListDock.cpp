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

#include "FunctionListDock.h"

#include "FunctionListParser.h"
#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>


static const int LineRole = Qt::UserRole;

FunctionListDock::FunctionListDock(MainWindow *window) :
    QDockWidget(window),
    window(window)
{
    setWindowTitle(tr("Function List"));
    setObjectName(QStringLiteral("FunctionListDock"));

    QWidget *container = new QWidget(this);

    filterEdit = new QLineEdit();
    filterEdit->setPlaceholderText(tr("Search"));
    filterEdit->setClearButtonEnabled(true);
    connect(filterEdit, &QLineEdit::textChanged, this, &FunctionListDock::applyFilter);

    sortButton = new QToolButton();
    sortButton->setText(QStringLiteral("A") + QChar(0x2193) + QStringLiteral("Z"));
    sortButton->setToolTip(tr("Sort alphabetically"));
    sortButton->setCheckable(true);
    sortButton->setAutoRaise(true);
    connect(sortButton, &QToolButton::toggled, this, &FunctionListDock::refresh);

    QToolButton *refreshButton = new QToolButton();
    refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refreshButton->setToolTip(tr("Reload"));
    refreshButton->setAutoRaise(true);
    connect(refreshButton, &QToolButton::clicked, this, &FunctionListDock::refresh);

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->addWidget(filterEdit, 1);
    toolbar->addWidget(sortButton);
    toolbar->addWidget(refreshButton);

    tree = new QTreeWidget();
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setUniformRowHeights(true);
    tree->setExpandsOnDoubleClick(false);
    connect(tree, &QTreeWidget::itemClicked, this, &FunctionListDock::goToItem);
    connect(tree, &QTreeWidget::itemActivated, this, &FunctionListDock::goToItem);

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addLayout(toolbar);
    layout->addWidget(tree);

    setWidget(container);

    refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(750);
    connect(refreshTimer, &QTimer::timeout, this, &FunctionListDock::refresh);

    connect(window, &MainWindow::editorActivated, this, &FunctionListDock::setEditor);
}

void FunctionListDock::showEvent(QShowEvent *event)
{
    QDockWidget::showEvent(event);

    setEditor(window->currentEditor());
}

void FunctionListDock::setEditor(ScintillaNext *newEditor)
{
    for (const QMetaObject::Connection &connection : editorConnections) {
        disconnect(connection);
    }
    editorConnections.clear();

    editor = newEditor;

    if (!editor) {
        tree->clear();
        return;
    }

    editorConnections.append(connect(editor, &ScintillaNext::updateUi, this, [=](Scintilla::Update updated) {
        if (Scintilla::FlagSet(updated, Scintilla::Update::Content) && isVisible()) {
            refreshTimer->start();
        }
    }));
    editorConnections.append(connect(editor, &ScintillaNext::lexerChanged, this, &FunctionListDock::refresh));

    refresh();
}

static QTreeWidgetItem *createItem(const FunctionListParser::Entry &entry)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, entry.name);
    item->setData(0, LineRole, entry.line);
    item->setToolTip(0, QObject::tr("Line %1").arg(entry.line + 1));

    if (entry.isContainer) {
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
    }

    return item;
}

void FunctionListDock::refresh()
{
    if (!editor || !isVisible()) {
        return;
    }

    // Remember what was collapsed so it stays that way
    QSet<QString> collapsed;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->childCount() > 0 && !(*it)->isExpanded()) {
            collapsed.insert((*it)->text(0));
        }
    }

    tree->clear();

    const QString language = editor->languageName;
    if (!FunctionListParser::supportsLanguage(language)) {
        QTreeWidgetItem *item = new QTreeWidgetItem(tree);
        item->setText(0, tr("(Function list is not available for %1)").arg(language));
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    const QString text = QString::fromUtf8(reinterpret_cast<const char *>(editor->characterPointer()), editor->textLength());
    const QVector<FunctionListParser::Entry> entries = FunctionListParser::parse(language, text);

    // Root item with the file name, like Notepad++
    QTreeWidgetItem *root = new QTreeWidgetItem(tree);
    root->setText(0, editor->getName());
    root->setData(0, LineRole, -1);
    QFont rootFont = root->font(0);
    rootFont.setBold(true);
    root->setFont(0, rootFont);

    QHash<QString, QTreeWidgetItem *> containers;
    QList<QPair<int, QTreeWidgetItem *>> headingStack; // For markdown style nesting

    for (const FunctionListParser::Entry &entry : entries) {
        if (entry.level > 0) {
            while (!headingStack.isEmpty() && headingStack.last().first >= entry.level) {
                headingStack.removeLast();
            }

            QTreeWidgetItem *item = createItem(entry);
            (headingStack.isEmpty() ? root : headingStack.last().second)->addChild(item);
            headingStack.append({entry.level, item});
            continue;
        }

        if (entry.isContainer) {
            if (!containers.contains(entry.name)) {
                QTreeWidgetItem *item = createItem(entry);
                root->addChild(item);
                containers.insert(entry.name, item);
            }
            continue;
        }

        QTreeWidgetItem *item = createItem(entry);

        if (!entry.container.isEmpty()) {
            QTreeWidgetItem *parent = containers.value(entry.container);

            if (parent == Q_NULLPTR) {
                // e.g. C++ methods defined outside of their class: Foo::bar()
                FunctionListParser::Entry containerEntry;
                containerEntry.name = entry.container;
                containerEntry.line = entry.line;
                containerEntry.isContainer = true;

                parent = createItem(containerEntry);
                root->addChild(parent);
                containers.insert(entry.container, parent);
            }

            parent->addChild(item);
        }
        else {
            root->addChild(item);
        }
    }

    if (sortButton->isChecked()) {
        tree->sortItems(0, Qt::AscendingOrder);
    }

    tree->expandAll();
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it) != root && collapsed.contains((*it)->text(0))) {
            (*it)->setExpanded(false);
        }
    }

    applyFilter(filterEdit->text());
}

void FunctionListDock::applyFilter(const QString &text)
{
    // An item is visible if it matches or any of its children match
    std::function<bool(QTreeWidgetItem *)> filterItem = [&](QTreeWidgetItem *item) -> bool {
        bool childVisible = false;

        for (int i = 0; i < item->childCount(); ++i) {
            childVisible |= filterItem(item->child(i));
        }

        const bool matches = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
        const bool visible = matches || childVisible;

        item->setHidden(!visible);

        return visible;
    };

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = tree->topLevelItem(i);
        filterItem(item);
        item->setHidden(false); // Always show the root
    }
}

void FunctionListDock::goToItem(QTreeWidgetItem *item)
{
    if (!editor || item == Q_NULLPTR) {
        return;
    }

    const QVariant data = item->data(0, LineRole);
    if (!data.isValid() || data.toInt() < 0) {
        return;
    }

    const int line = data.toInt();

    editor->ensureVisibleEnforcePolicy(line);
    editor->gotoLine(line);
    editor->verticalCentreCaret();
    editor->grabFocus();
}
