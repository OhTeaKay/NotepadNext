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

#include "TextEncoding.h"

#include "uchardet.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QTextCodec>

#include <memory>


namespace TextEncoding {

bool isValidUtf8(const QByteArray &data, bool allowTruncatedEnd)
{
    const unsigned char *s = reinterpret_cast<const unsigned char *>(data.constData());
    const qsizetype len = data.size();
    qsizetype i = 0;

    while (i < len) {
        const unsigned char c = s[i];

        if (c < 0x80) {
            ++i;
            continue;
        }

        int extra;
        unsigned int codePoint;

        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            codePoint = c & 0x1F;
        }
        else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            codePoint = c & 0x0F;
        }
        else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            codePoint = c & 0x07;
        }
        else {
            return false;
        }

        if (i + extra >= len) {
            // Not enough bytes left for the full sequence
            if (!allowTruncatedEnd) {
                return false;
            }

            // Make sure what is there is at least plausible
            for (qsizetype j = i + 1; j < len; ++j) {
                if ((s[j] & 0xC0) != 0x80) {
                    return false;
                }
            }

            return true;
        }

        for (int j = 1; j <= extra; ++j) {
            if ((s[i + j] & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (s[i + j] & 0x3F);
        }

        // Reject overlong encodings, surrogates and out of range values
        if ((extra == 1 && codePoint < 0x80) ||
            (extra == 2 && codePoint < 0x800) ||
            (extra == 3 && codePoint < 0x10000) ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF) ||
            codePoint > 0x10FFFF) {
            return false;
        }

        i += extra + 1;
    }

    return true;
}

bool isAscii(const QByteArray &data)
{
    for (const char c : data) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            return false;
        }
    }

    return true;
}

QByteArray guessEncoding(const QByteArray &data)
{
    uchardet_t ud = uchardet_new();

    uchardet_handle_data(ud, data.constData(), data.size());
    uchardet_data_end(ud);

    QByteArray charset(uchardet_get_charset(ud));

    uchardet_delete(ud);

    if (charset.isEmpty() || charset.compare("ASCII", Qt::CaseInsensitive) == 0) {
        return QByteArray();
    }

    // uchardet uses some names Qt does not know about
    static const QHash<QByteArray, QByteArray> aliases = {
        {"MAC-CYRILLIC", "x-mac-cyrillic"},
        {"MAC-CENTRALEUROPE", "x-mac-ce"},
        {"VISCII", "windows-1258"},
        {"X-ISO-10646-UCS-4-3412", QByteArray()},
        {"X-ISO-10646-UCS-4-2143", QByteArray()},
    };

    const QByteArray upper = charset.toUpper();
    if (aliases.contains(upper)) {
        charset = aliases.value(upper);
    }

    if (charset.isEmpty() || !isAvailable(charset)) {
        return QByteArray();
    }

    return charset;
}

QByteArray ansiCodecName()
{
    QTextCodec *codec = QTextCodec::codecForLocale();

    if (codec && !isUtf8(codec->name())) {
        return codec->name();
    }

    // macOS and Linux use UTF-8 for everything, so use the same "ANSI" code page Windows
    // would use for the user's language. This way files coming from Windows are read correctly.
    static const QHash<QLocale::Language, QByteArray> windowsCodePages = {
        {QLocale::Polish, "windows-1250"}, {QLocale::Czech, "windows-1250"}, {QLocale::Slovak, "windows-1250"},
        {QLocale::Hungarian, "windows-1250"}, {QLocale::Slovenian, "windows-1250"}, {QLocale::Croatian, "windows-1250"},
        {QLocale::Romanian, "windows-1250"}, {QLocale::Albanian, "windows-1250"}, {QLocale::Bosnian, "windows-1250"},
        {QLocale::Russian, "windows-1251"}, {QLocale::Ukrainian, "windows-1251"}, {QLocale::Belarusian, "windows-1251"},
        {QLocale::Bulgarian, "windows-1251"}, {QLocale::Serbian, "windows-1251"}, {QLocale::Macedonian, "windows-1251"},
        {QLocale::Kazakh, "windows-1251"},
        {QLocale::Greek, "windows-1253"},
        {QLocale::Turkish, "windows-1254"}, {QLocale::Azerbaijani, "windows-1254"},
        {QLocale::Hebrew, "windows-1255"},
        {QLocale::Arabic, "windows-1256"}, {QLocale::Persian, "windows-1256"}, {QLocale::Urdu, "windows-1256"},
        {QLocale::Lithuanian, "windows-1257"}, {QLocale::Latvian, "windows-1257"}, {QLocale::Estonian, "windows-1257"},
        {QLocale::Vietnamese, "windows-1258"},
        {QLocale::Thai, "TIS-620"},
        {QLocale::Japanese, "Shift_JIS"},
        {QLocale::Korean, "windows-949"},
    };

    const QLocale locale = QLocale::system();
    QByteArray name = windowsCodePages.value(locale.language());

    if (locale.language() == QLocale::Chinese) {
        name = (locale.script() == QLocale::TraditionalChineseScript) ? QByteArrayLiteral("Big5") : QByteArrayLiteral("GBK");
    }

    if (!name.isEmpty() && isAvailable(name)) {
        return name;
    }

    return QByteArrayLiteral("windows-1252");
}

QTextCodec *codecForName(const QByteArray &name)
{
    if (name.isEmpty()) {
        return Q_NULLPTR;
    }

    return QTextCodec::codecForName(name);
}

bool isAvailable(const QByteArray &name)
{
    return codecForName(name) != Q_NULLPTR;
}

QByteArray canonicalName(const QByteArray &name)
{
    QTextCodec *codec = codecForName(name);

    return codec ? codec->name() : name;
}

bool isUtf8(const QByteArray &name)
{
    if (name.isEmpty()) {
        return true;
    }

    const QByteArray n = name.toUpper();
    return n == "UTF-8" || n == "UTF8";
}

QString displayName(const QByteArray &codecName)
{
    if (isUtf8(codecName)) {
        return QStringLiteral("UTF-8");
    }

    // Look through the "pretty" names used in the menu
    const QByteArray canonical = canonicalName(codecName);
    for (const CharacterSetGroup &group : characterSetGroups()) {
        for (const CharacterSet &set : group.sets) {
            if (canonicalName(set.codecName) == canonical) {
                return set.displayName;
            }
        }
    }

    QString name = QString::fromLatin1(canonical);
    if (name.startsWith(QLatin1String("windows-"), Qt::CaseInsensitive)) {
        name[0] = QLatin1Char('W');
    }

    return name;
}

QList<CharacterSetGroup> characterSetGroups()
{
    // Mirrors Notepad++'s "Encoding > Character sets" menu
    static const QList<CharacterSetGroup> groups = {
        {QCoreApplication::translate("TextEncoding", "Arabic"), {
            {QStringLiteral("ISO 8859-6"), "ISO-8859-6"},
            {QStringLiteral("OEM 720"), "IBM720"},
            {QStringLiteral("Windows-1256"), "windows-1256"},
        }},
        {QCoreApplication::translate("TextEncoding", "Baltic"), {
            {QStringLiteral("ISO 8859-4"), "ISO-8859-4"},
            {QStringLiteral("ISO 8859-13"), "ISO-8859-13"},
            {QStringLiteral("OEM 775"), "IBM775"},
            {QStringLiteral("Windows-1257"), "windows-1257"},
        }},
        {QCoreApplication::translate("TextEncoding", "Celtic"), {
            {QStringLiteral("ISO 8859-14"), "ISO-8859-14"},
        }},
        {QCoreApplication::translate("TextEncoding", "Cyrillic"), {
            {QStringLiteral("ISO 8859-5"), "ISO-8859-5"},
            {QStringLiteral("KOI8-R"), "KOI8-R"},
            {QStringLiteral("KOI8-U"), "KOI8-U"},
            {QStringLiteral("Macintosh"), "x-mac-cyrillic"},
            {QStringLiteral("OEM 855"), "IBM855"},
            {QStringLiteral("OEM 866"), "IBM866"},
            {QStringLiteral("Windows-1251"), "windows-1251"},
        }},
        {QCoreApplication::translate("TextEncoding", "Central European"), {
            {QStringLiteral("OEM 852"), "IBM852"},
            {QStringLiteral("Windows-1250"), "windows-1250"},
        }},
        {QCoreApplication::translate("TextEncoding", "Chinese"), {
            {QStringLiteral("Big5 (Traditional)"), "Big5"},
            {QStringLiteral("GB2312 (Simplified)"), "GB2312"},
            {QStringLiteral("GB18030"), "GB18030"},
        }},
        {QCoreApplication::translate("TextEncoding", "Eastern European"), {
            {QStringLiteral("ISO 8859-2"), "ISO-8859-2"},
        }},
        {QCoreApplication::translate("TextEncoding", "Greek"), {
            {QStringLiteral("ISO 8859-7"), "ISO-8859-7"},
            {QStringLiteral("OEM 737"), "IBM737"},
            {QStringLiteral("OEM 869"), "IBM869"},
            {QStringLiteral("Windows-1253"), "windows-1253"},
        }},
        {QCoreApplication::translate("TextEncoding", "Hebrew"), {
            {QStringLiteral("ISO 8859-8"), "ISO-8859-8"},
            {QStringLiteral("OEM 862"), "IBM862"},
            {QStringLiteral("Windows-1255"), "windows-1255"},
        }},
        {QCoreApplication::translate("TextEncoding", "Japanese"), {
            {QStringLiteral("Shift-JIS"), "Shift_JIS"},
            {QStringLiteral("EUC-JP"), "EUC-JP"},
        }},
        {QCoreApplication::translate("TextEncoding", "Korean"), {
            {QStringLiteral("Windows 949"), "windows-949"},
            {QStringLiteral("EUC-KR"), "EUC-KR"},
        }},
        {QCoreApplication::translate("TextEncoding", "North European"), {
            {QStringLiteral("OEM 861 : Icelandic"), "IBM861"},
            {QStringLiteral("OEM 865 : Nordic"), "IBM865"},
        }},
        {QCoreApplication::translate("TextEncoding", "Thai"), {
            {QStringLiteral("TIS-620"), "TIS-620"},
        }},
        {QCoreApplication::translate("TextEncoding", "Turkish"), {
            {QStringLiteral("ISO 8859-3"), "ISO-8859-3"},
            {QStringLiteral("ISO 8859-9"), "ISO-8859-9"},
            {QStringLiteral("OEM 857"), "IBM857"},
            {QStringLiteral("Windows-1254"), "windows-1254"},
        }},
        {QCoreApplication::translate("TextEncoding", "Western European"), {
            {QStringLiteral("ISO 8859-1"), "ISO-8859-1"},
            {QStringLiteral("ISO 8859-15"), "ISO-8859-15"},
            {QStringLiteral("OEM 850"), "IBM850"},
            {QStringLiteral("OEM 858"), "IBM00858"},
            {QStringLiteral("OEM 860 : Portuguese"), "IBM860"},
            {QStringLiteral("OEM 863 : French"), "IBM863"},
            {QStringLiteral("OEM-US"), "IBM437"},
            {QStringLiteral("Macintosh (Roman)"), "macintosh"},
            {QStringLiteral("Windows-1252"), "windows-1252"},
        }},
        {QCoreApplication::translate("TextEncoding", "Vietnamese"), {
            {QStringLiteral("Windows-1258"), "windows-1258"},
        }},
    };

    return groups;
}

QString decode(const QByteArray &data, QByteArray *encoding, QByteArray *bom)
{
    static const QByteArray BOM_UTF8 = QByteArray::fromHex("EFBBBF");
    static const QByteArray BOM_UTF16LE = QByteArray::fromHex("FFFE");
    static const QByteArray BOM_UTF16BE = QByteArray::fromHex("FEFF");

    QByteArray detectedEncoding;
    QByteArray detectedBom;

    if (data.startsWith(BOM_UTF8)) {
        detectedEncoding = UTF8;
        detectedBom = BOM_UTF8;
    }
    else if (data.startsWith(BOM_UTF16LE)) {
        detectedEncoding = UTF16LE;
        detectedBom = BOM_UTF16LE;
    }
    else if (data.startsWith(BOM_UTF16BE)) {
        detectedEncoding = UTF16BE;
        detectedBom = BOM_UTF16BE;
    }
    else if (isValidUtf8(data)) {
        detectedEncoding = UTF8;
    }
    else {
        detectedEncoding = guessEncoding(data);

        if (detectedEncoding.isEmpty()) {
            detectedEncoding = ansiCodecName();
        }
    }

    if (encoding) *encoding = detectedEncoding;
    if (bom) *bom = detectedBom;

    const QByteArray content = data.mid(detectedBom.size());

    if (isUtf8(detectedEncoding)) {
        return QString::fromUtf8(content);
    }

    QTextCodec *codec = codecForName(detectedEncoding);
    if (codec == Q_NULLPTR) {
        return QString::fromUtf8(content);
    }

    std::unique_ptr<QTextDecoder> decoder(codec->makeDecoder(QTextCodec::IgnoreHeader));
    return decoder->toUnicode(content);
}

QByteArray encode(const QString &text, const QByteArray &encoding)
{
    if (isUtf8(encoding)) {
        return text.toUtf8();
    }

    QTextCodec *codec = codecForName(encoding);
    if (codec == Q_NULLPTR) {
        return text.toUtf8();
    }

    std::unique_ptr<QTextEncoder> encoder(codec->makeEncoder(QTextCodec::IgnoreHeader));
    return encoder->fromUnicode(text);
}

}
