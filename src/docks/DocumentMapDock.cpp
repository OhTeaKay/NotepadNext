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

#include "DocumentMapDock.h"

#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>


DocumentMapDock::DocumentMapDock(MainWindow *window) :
    QDockWidget(window),
    window(window)
{
    setWindowTitle(tr("Document Map"));
    setObjectName(QStringLiteral("DocumentMapDock"));

    map = new ScintillaEdit(this);
    map->setFocusPolicy(Qt::NoFocus);
    map->setAcceptDrops(false);
    map->viewport()->setAcceptDrops(false);
    map->viewport()->setCursor(Qt::PointingHandCursor);

    for (int i = 0; i <= SC_MAX_MARGIN; ++i) {
        map->setMarginWidthN(i, 0);
    }

    map->setHScrollBar(false);
    map->setVScrollBar(false);
    map->setCaretStyle(CARETSTYLE_INVISIBLE);
    map->setCaretLineVisibleAlways(false);
    map->setWrapMode(SC_WRAP_NONE);
    map->setEndAtLastLine(false);
    map->setZoom(-10);

    for (int i = 0; i <= INDICATOR_MAX; ++i) {
        map->indicSetStyle(i, INDIC_HIDDEN);
    }

    map->viewport()->installEventFilter(this);
    map->installEventFilter(this);

    viewRegion = new QWidget(map->viewport());
    viewRegion->setAttribute(Qt::WA_TransparentForMouseEvents);
    viewRegion->setAttribute(Qt::WA_StyledBackground);
    viewRegion->hide();

    setWidget(map);

    connect(window, &MainWindow::editorActivated, this, &DocumentMapDock::setEditor);
}

DocumentMapDock::~DocumentMapDock()
{
    detachDocument();
}

void DocumentMapDock::detachDocument()
{
    for (const QMetaObject::Connection &connection : editorConnections) {
        disconnect(connection);
    }
    editorConnections.clear();

    if (editor) {
        editor->removeEventFilter(this);
    }

    editor.clear();

    // Give the map its own empty document so it does not keep the old one alive
    map->setDocPointer(0);
    viewRegion->hide();
}

void DocumentMapDock::setEditor(ScintillaNext *newEditor)
{
    if (!isVisible()) {
        // Remember it for later, but don't do any work while hidden
        if (editor != newEditor) {
            detachDocument();
            editor = newEditor;
        }
        return;
    }

    if (editor == newEditor && map->docPointer() == (editor ? editor->docPointer() : 0)) {
        updateViewRegion();
        return;
    }

    detachDocument();

    editor = newEditor;

    if (!editor) {
        return;
    }

    map->setDocPointer(editor->docPointer());
    syncStyles();

    editorConnections.append(connect(editor, &ScintillaNext::updateUi, this, [=](Scintilla::Update) { updateViewRegion(); }));
    editorConnections.append(connect(editor, &ScintillaNext::lexerChanged, this, &DocumentMapDock::syncStyles));
    editorConnections.append(connect(editor, &ScintillaNext::closed, this, &DocumentMapDock::detachDocument));
    editor->installEventFilter(this);

    updateViewRegion();
}

void DocumentMapDock::syncStyles()
{
    if (!editor) {
        return;
    }

    // Style definitions are per view, the styling information itself is part of the shared document
    for (int i = 0; i <= STYLE_MAX; ++i) {
        map->styleSetFore(i, editor->styleFore(i));
        map->styleSetBack(i, editor->styleBack(i));
        map->styleSetBold(i, editor->styleBold(i));
        map->styleSetItalic(i, editor->styleItalic(i));
        map->styleSetEOLFilled(i, editor->styleEOLFilled(i));
        map->styleSetSize(i, editor->styleSize(i));
        map->styleSetFont(i, editor->styleFont(i).constData());
        map->styleSetVisible(i, editor->styleVisible(i));
    }

    map->setTabWidth(editor->tabWidth());
    map->setViewWS(SCWS_INVISIBLE);
    map->setViewEOL(false);
    map->setIndentationGuides(SC_IV_NONE);
    map->setEdgeMode(EDGE_NONE);

    updateViewRegion();
}

void DocumentMapDock::updateViewRegion()
{
    if (!editor || !isVisible()) {
        return;
    }

    const int lineHeight = qMax(1, static_cast<int>(map->textHeight(0)));
    const int totalLines = editor->lineCount();
    const int mapLinesOnScreen = qMax(1, map->viewport()->height() / lineHeight);

    const int editorFirstVisible = editor->firstVisibleLine();
    const int firstDocLine = editor->docLineFromVisible(editorFirstVisible);
    const int lastDocLine = qMin(totalLines - 1, static_cast<int>(editor->docLineFromVisible(editorFirstVisible + editor->linesOnScreen() - 1)));
    const int visibleDocLines = qMax(1, lastDocLine - firstDocLine + 1);

    // Scroll the map proportionally so the visible region stays in view
    int mapFirstLine = 0;
    if (totalLines > mapLinesOnScreen) {
        const int scrollableEditorLines = qMax(1, totalLines - visibleDocLines);
        const double ratio = qBound(0.0, static_cast<double>(firstDocLine) / scrollableEditorLines, 1.0);
        mapFirstLine = qRound(ratio * (totalLines - mapLinesOnScreen));
    }
    map->setFirstVisibleLine(mapFirstLine);

    const QColor highlight = palette().color(QPalette::Highlight);
    viewRegion->setStyleSheet(QStringLiteral("background-color: rgba(%1, %2, %3, 50); border: 1px solid rgba(%1, %2, %3, 160);")
                                  .arg(highlight.red()).arg(highlight.green()).arg(highlight.blue()));

    const int y = (firstDocLine - mapFirstLine) * lineHeight;
    const int height = qMax(lineHeight, visibleDocLines * lineHeight);
    viewRegion->setGeometry(0, y, map->viewport()->width(), height);
    viewRegion->show();
}

void DocumentMapDock::scrollEditorTo(int y)
{
    if (!editor) {
        return;
    }

    const int lineHeight = qMax(1, static_cast<int>(map->textHeight(0)));
    const int docLine = qBound(0, static_cast<int>(map->firstVisibleLine()) + y / lineHeight, static_cast<int>(editor->lineCount()) - 1);
    const int visibleLine = editor->visibleFromDocLine(docLine);

    editor->setFirstVisibleLine(qMax(0, visibleLine - static_cast<int>(editor->linesOnScreen()) / 2));
}

bool DocumentMapDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == map->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                dragging = true;
                scrollEditorTo(mouseEvent->position().toPoint().y());
            }
            return true;
        }
        case QEvent::MouseMove: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (dragging) {
                scrollEditorTo(mouseEvent->position().toPoint().y());
            }
            return true;
        }
        case QEvent::MouseButtonRelease:
            dragging = false;
            return true;
        case QEvent::Wheel: {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            if (editor) {
                editor->lineScroll(0, -wheelEvent->angleDelta().y() / 40);
            }
            return true;
        }
        case QEvent::Resize:
            QMetaObject::invokeMethod(this, &DocumentMapDock::updateViewRegion, Qt::QueuedConnection);
            break;
        case QEvent::ContextMenu:
        case QEvent::DragEnter:
        case QEvent::Drop:
            return true;
        default:
            break;
        }
    }
    else if (watched == map) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease || event->type() == QEvent::InputMethod) {
            return true;
        }
    }
    else if (watched == editor && event->type() == QEvent::Resize) {
        QMetaObject::invokeMethod(this, &DocumentMapDock::updateViewRegion, Qt::QueuedConnection);
    }

    return QDockWidget::eventFilter(watched, event);
}

void DocumentMapDock::showEvent(QShowEvent *event)
{
    QDockWidget::showEvent(event);

    ScintillaNext *current = window->currentEditor();

    // Force everything to be set up again now that it is visible
    detachDocument();
    setEditor(current);
}
