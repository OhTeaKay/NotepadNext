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

#include "ScrollSynchronizer.h"

#include "ScintillaNext.h"


ScrollSynchronizer::ScrollSynchronizer(QObject *parent) :
    QObject(parent)
{
}

void ScrollSynchronizer::setVertical(bool enabled)
{
    vertical = enabled;
    resetOffsets();
    connectEditors();
}

void ScrollSynchronizer::setHorizontal(bool enabled)
{
    horizontal = enabled;
    resetOffsets();
    connectEditors();
}

void ScrollSynchronizer::setEditors(ScintillaNext *a, ScintillaNext *b)
{
    if ((a == first && b == second) || (a == second && b == first)) {
        return;
    }

    first = a;
    second = b;

    resetOffsets();
    connectEditors();
}

void ScrollSynchronizer::resetOffsets()
{
    if (first && second) {
        lineOffset = second->firstVisibleLine() - first->firstVisibleLine();
        xOffset = second->xOffset() - first->xOffset();
    }
}

void ScrollSynchronizer::connectEditors()
{
    for (const QMetaObject::Connection &connection : connections) {
        disconnect(connection);
    }
    connections.clear();

    if (!isEnabled() || !first || !second || first == second) {
        return;
    }

    connections.append(connect(first, &ScintillaNext::updateUi, this, [=](Scintilla::Update updated) {
        if (Scintilla::FlagSet(updated, Scintilla::Update::VScroll) || Scintilla::FlagSet(updated, Scintilla::Update::HScroll)) {
            synchronize(first, second, 1);
        }
    }));

    connections.append(connect(second, &ScintillaNext::updateUi, this, [=](Scintilla::Update updated) {
        if (Scintilla::FlagSet(updated, Scintilla::Update::VScroll) || Scintilla::FlagSet(updated, Scintilla::Update::HScroll)) {
            synchronize(second, first, -1);
        }
    }));
}

void ScrollSynchronizer::synchronize(ScintillaNext *source, ScintillaNext *target, int sign)
{
    if (syncing || !source || !target) {
        return;
    }

    syncing = true;

    if (vertical) {
        const int line = source->firstVisibleLine() + sign * lineOffset;
        if (line != target->firstVisibleLine()) {
            target->setFirstVisibleLine(qMax(0, line));
        }
    }

    if (horizontal) {
        const int x = source->xOffset() + sign * xOffset;
        if (x != target->xOffset()) {
            target->setXOffset(qMax(0, x));
        }
    }

    syncing = false;
}
