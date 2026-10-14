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

#include "CharacterPanelDock.h"

#include "MainWindow.h"
#include "ScintillaNext.h"

#include <QHash>
#include <QHeaderView>
#include <QTableWidget>
#include <QTextCodec>


static const char *const CONTROL_NAMES[] = {
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS", "TAB", "LF", "VT", "FF", "CR", "SO", "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};

static const char *const LATIN1_ENTITIES[] = {
    "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy", "ordf", "laquo", "not", "shy", "reg", "macr",
    "deg", "plusmn", "sup2", "sup3", "acute", "micro", "para", "middot", "cedil", "sup1", "ordm", "raquo", "frac14", "frac12", "frac34", "iquest",
    "Agrave", "Aacute", "Acirc", "Atilde", "Auml", "Aring", "AElig", "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute", "Icirc", "Iuml",
    "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash", "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig",
    "agrave", "aacute", "acirc", "atilde", "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc", "euml", "igrave", "iacute", "icirc", "iuml",
    "eth", "ntilde", "ograve", "oacute", "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave", "uacute", "ucirc", "uuml", "yacute", "thorn", "yuml"
};

static QString entityName(uint codePoint)
{
    static const QHash<uint, QString> names = {
        {34, QStringLiteral("quot")}, {38, QStringLiteral("amp")}, {39, QStringLiteral("apos")}, {60, QStringLiteral("lt")}, {62, QStringLiteral("gt")},
        {8364, QStringLiteral("euro")}, {8218, QStringLiteral("sbquo")}, {402, QStringLiteral("fnof")}, {8222, QStringLiteral("bdquo")},
        {8230, QStringLiteral("hellip")}, {8224, QStringLiteral("dagger")}, {8225, QStringLiteral("Dagger")}, {710, QStringLiteral("circ")},
        {8240, QStringLiteral("permil")}, {352, QStringLiteral("Scaron")}, {8249, QStringLiteral("lsaquo")}, {338, QStringLiteral("OElig")},
        {381, QStringLiteral("Zcaron")}, {8216, QStringLiteral("lsquo")}, {8217, QStringLiteral("rsquo")}, {8220, QStringLiteral("ldquo")},
        {8221, QStringLiteral("rdquo")}, {8226, QStringLiteral("bull")}, {8211, QStringLiteral("ndash")}, {8212, QStringLiteral("mdash")},
        {732, QStringLiteral("tilde")}, {8482, QStringLiteral("trade")}, {353, QStringLiteral("scaron")}, {8250, QStringLiteral("rsaquo")},
        {339, QStringLiteral("oelig")}, {382, QStringLiteral("zcaron")}, {376, QStringLiteral("Yuml")},
    };

    if (codePoint >= 160 && codePoint <= 255) {
        return QString::fromLatin1(LATIN1_ENTITIES[codePoint - 160]);
    }

    return names.value(codePoint);
}

CharacterPanelDock::CharacterPanelDock(MainWindow *window) :
    QDockWidget(window),
    window(window)
{
    setWindowTitle(tr("ASCII Codes Insertion Panel"));
    setObjectName(QStringLiteral("CharacterPanelDock"));

    table = new QTableWidget(256, 6, this);
    table->setHorizontalHeaderLabels({tr("Value"), tr("Hex"), tr("Character"), tr("HTML Name"), tr("HTML Decimal"), tr("HTML Hexadecimal")});
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 4);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setToolTip(tr("Double click to insert the value into the document"));
    connect(table, &QTableWidget::cellDoubleClicked, this, &CharacterPanelDock::insertCell);

    populate();

    table->resizeColumnsToContents();

    setWidget(table);
}

void CharacterPanelDock::populate()
{
    // Values 128-255 are shown as Windows-1252 like Notepad++ does on most western systems
    QTextCodec *codec = QTextCodec::codecForName("windows-1252");

    for (int value = 0; value < 256; ++value) {
        QString character;
        uint codePoint = value;

        if (value < 32) {
            character = QString::fromLatin1(CONTROL_NAMES[value]);
        }
        else if (value == 127) {
            character = QStringLiteral("DEL");
        }
        else if (value >= 128 && codec) {
            const char byte = static_cast<char>(value);
            const QString decoded = codec->toUnicode(&byte, 1);
            codePoint = decoded.isEmpty() ? value : decoded.at(0).unicode();
            character = decoded;
        }
        else {
            character = QChar(value);
        }

        const QString name = entityName(codePoint);

        auto item = [](const QString &text, uint codePoint) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setData(Qt::UserRole, codePoint);
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        table->setItem(value, 0, item(QString::number(value), codePoint));
        table->setItem(value, 1, item(QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(), codePoint));
        table->setItem(value, 2, item(character, codePoint));
        table->setItem(value, 3, item(name.isEmpty() ? QString() : QStringLiteral("&%1;").arg(name), codePoint));
        table->setItem(value, 4, item(value >= 32 ? QStringLiteral("&#%1;").arg(codePoint) : QString(), codePoint));
        table->setItem(value, 5, item(value >= 32 ? QStringLiteral("&#x%1;").arg(codePoint, 0, 16).toUpper().replace(QLatin1String("&#X"), QLatin1String("&#x")) : QString(), codePoint));
    }
}

void CharacterPanelDock::insertCell(int row, int column)
{
    ScintillaNext *editor = window->currentEditor();

    if (editor == Q_NULLPTR || editor->readOnly()) {
        return;
    }

    QTableWidgetItem *item = table->item(row, column);
    QString text;

    if (column >= 3) {
        text = item->text();
    }

    if (text.isEmpty()) {
        // Insert the character itself
        const uint codePoint = item->data(Qt::UserRole).toUInt();
        text = QString::fromUcs4(reinterpret_cast<const char32_t *>(&codePoint), 1);
    }

    const QByteArray utf8 = text.toUtf8();

    // Use addText so NUL characters can be inserted too
    if (!editor->selectionEmpty()) {
        editor->replaceSel("");
    }
    editor->addText(utf8.size(), utf8.constData());
    editor->grabFocus();
}
