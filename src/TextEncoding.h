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

#include <QByteArray>
#include <QList>
#include <QString>

class QTextCodec;

// Helpers for detecting and converting text encodings, similar to what Notepad++ offers
// in its "Encoding" menu. The editor buffer itself is always UTF-8, files are converted
// from/to their encoding when they are read/written.
namespace TextEncoding {

inline const QByteArray UTF8 = QByteArrayLiteral("UTF-8");
inline const QByteArray UTF16LE = QByteArrayLiteral("UTF-16LE");
inline const QByteArray UTF16BE = QByteArrayLiteral("UTF-16BE");

struct CharacterSet {
    QString displayName;
    QByteArray codecName;
};

struct CharacterSetGroup {
    QString name;
    QList<CharacterSet> sets;
};

// Returns true if the data is valid UTF-8. If allowTruncatedEnd is set, an incomplete
// multi-byte sequence at the very end of the data is not considered an error (useful when
// the data is just the first chunk of a larger file).
bool isValidUtf8(const QByteArray &data, bool allowTruncatedEnd = false);

// Returns true if the data contains only 7-bit ASCII characters
bool isAscii(const QByteArray &data);

// Tries to guess the encoding of some data that is known *not* to be UTF-8.
// Returns an empty array if nothing reasonable could be determined.
QByteArray guessEncoding(const QByteArray &data);

// The encoding used for "ANSI" files: the system code page if it is not UTF-8, else Windows-1252
QByteArray ansiCodecName();

// Look up a codec, returns nullptr if it is not available on this platform
QTextCodec *codecForName(const QByteArray &name);
bool isAvailable(const QByteArray &name);

// Normalized name used to compare codecs (e.g. "windows-1250" == "CP1250")
QByteArray canonicalName(const QByteArray &name);
bool isUtf8(const QByteArray &name);

// Human readable name, e.g. "Windows-1250" or "Shift-JIS"
QString displayName(const QByteArray &codecName);

// The Notepad++ "Character sets" menu
QList<CharacterSetGroup> characterSetGroups();

// Decodes raw file data using the same rules as the editor (BOM, UTF-8 validation, guessing).
// The detected encoding and BOM bytes are optionally returned.
QString decode(const QByteArray &data, QByteArray *encoding = nullptr, QByteArray *bom = nullptr);

// Encodes text into the given encoding (no BOM is added)
QByteArray encode(const QString &text, const QByteArray &encoding);

}
