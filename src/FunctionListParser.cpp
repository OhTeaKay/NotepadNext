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

#include "FunctionListParser.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <climits>


namespace FunctionListParser {

namespace {

enum class CommentStyle {
    None,
    CLike,      // // and /* */ plus "..." and '...' strings
    JavaScript, // same as CLike but also `...` strings
    Hash,       // # comments, "..." and '...' strings
    Python,     // # comments, triple quoted strings
    Lua,        // -- and --[[ ]] comments
    Sql,        // -- and /* */ comments, '...' strings
    Pascal,     // { }, (* *) and // comments, '...' strings
    VisualBasic,// ' comments, "..." strings
    Percent,    // % comments
    Semicolon,  // ; comments
};

enum class Strategy {
    CLikeScanner, // Heuristic scanner for C style function definitions
    Rules,        // Regular expression rules only
    Indentation,  // Regular expression rules, containers determined by indentation (Python, Ruby, ...)
    Markdown,     // Headings
};

struct Rule {
    QRegularExpression regex;
    int nameGroup = 1;
    int containerGroup = -1; // Group with the name of the container (e.g. Go method receivers)
    bool isContainer = false;
    int levelGroup = -1;     // Group whose length determines the nesting level (markdown)
    bool braceBody = false;  // For containers, the body is delimited by { }
};

struct LanguageDefinition {
    CommentStyle comments = CommentStyle::None;
    Strategy strategy = Strategy::Rules;
    QList<Rule> rules;
    bool preprocessor = false; // Treat lines starting with # as comments (C/C++)
    QString qualifierSeparator; // e.g. "::" for C++, "." for Lua and Pascal
};

Rule rule(const QString &pattern, int nameGroup = 1, bool isContainer = false, bool caseInsensitive = false)
{
    QRegularExpression::PatternOptions options = QRegularExpression::MultilineOption | QRegularExpression::UseUnicodePropertiesOption;
    if (caseInsensitive) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    Rule r;
    r.regex = QRegularExpression(pattern, options);
    r.nameGroup = nameGroup;
    r.isContainer = isContainer;
    return r;
}

Rule braceContainer(const QString &pattern, int nameGroup = 1)
{
    Rule r = rule(pattern, nameGroup, true);
    r.braceBody = true;
    return r;
}

const QHash<QString, LanguageDefinition> &definitions()
{
    static QHash<QString, LanguageDefinition> defs;

    if (!defs.isEmpty()) {
        return defs;
    }

    const QString cppContainers = QStringLiteral("\\b(?:class|struct|interface|record|union)\\s+(?:\\[\\[[^\\]]*\\]\\]\\s*)?(?:[A-Z_][A-Z0-9_]*\\s+)?([A-Za-z_]\\w*)");

    LanguageDefinition cLike;
    cLike.comments = CommentStyle::CLike;
    cLike.strategy = Strategy::CLikeScanner;
    cLike.preprocessor = true;
    cLike.qualifierSeparator = QStringLiteral("::");
    cLike.rules << braceContainer(cppContainers);
    defs.insert(QStringLiteral("C"), cLike);
    defs.insert(QStringLiteral("C++"), cLike);
    defs.insert(QStringLiteral("D"), cLike);

    LanguageDefinition objc = cLike;
    objc.rules << rule(QStringLiteral("^\\s*@(?:interface|implementation|protocol)\\s+(\\w+)"), 1, true)
               << rule(QStringLiteral("^\\s*[-+]\\s*\\([^)]*\\)\\s*(\\w+(?::\\s*\\([^)]*\\)\\s*\\w+(?:\\s+\\w+:\\s*\\([^)]*\\)\\s*\\w+)*)?)"));
    defs.insert(QStringLiteral("Objective-C"), objc);

    LanguageDefinition java;
    java.comments = CommentStyle::CLike;
    java.strategy = Strategy::CLikeScanner;
    java.rules << braceContainer(QStringLiteral("\\b(?:class|interface|enum|record|struct)\\s+([A-Za-z_]\\w*)"));
    defs.insert(QStringLiteral("Java"), java);
    defs.insert(QStringLiteral("C#"), java);

    LanguageDefinition js;
    js.comments = CommentStyle::JavaScript;
    js.strategy = Strategy::CLikeScanner;
    js.rules << braceContainer(QStringLiteral("\\bclass\\s+([A-Za-z_$][\\w$]*)"))
             << rule(QStringLiteral("\\b(?:const|let|var)\\s+([A-Za-z_$][\\w$]*)\\s*=\\s*(?:async\\s*)?(?:function\\b|\\([^()]*\\)\\s*=>|[A-Za-z_$][\\w$]*\\s*=>)"))
             << rule(QStringLiteral("^\\s*([A-Za-z_$][\\w$]*)\\s*:\\s*(?:async\\s*)?function\\b"));
    defs.insert(QStringLiteral("JavaScript"), js);
    defs.insert(QStringLiteral("ActionScript"), js);

    LanguageDefinition php;
    php.comments = CommentStyle::CLike;
    php.strategy = Strategy::CLikeScanner;
    php.rules << braceContainer(QStringLiteral("\\b(?:class|interface|trait|enum)\\s+([A-Za-z_]\\w*)"));
    defs.insert(QStringLiteral("PHP"), php);

    LanguageDefinition go;
    go.comments = CommentStyle::CLike;
    go.rules << braceContainer(QStringLiteral("^type\\s+(\\w+)\\s+(?:struct|interface)\\b"));
    Rule goFunc = rule(QStringLiteral("^func\\s+(?:\\(\\s*\\w*\\s*\\*?\\s*(\\w+)[^)]*\\)\\s*)?(\\w+)"), 2);
    goFunc.containerGroup = 1;
    go.rules << goFunc;
    defs.insert(QStringLiteral("Go"), go);

    LanguageDefinition rust;
    rust.comments = CommentStyle::CLike;
    rust.rules << braceContainer(QStringLiteral("^\\s*(?:pub(?:\\([^)]*\\))?\\s+)?(?:unsafe\\s+)?(?:impl(?:<[^>]*>)?\\s+(?:[\\w:<>,\\s]+\\s+for\\s+)?|trait\\s+|struct\\s+|enum\\s+|mod\\s+)([A-Za-z_]\\w*)"))
               << rule(QStringLiteral("^\\s*(?:pub(?:\\([^)]*\\))?\\s+)?(?:(?:const|async|unsafe|extern\\s+\"[^\"]*\")\\s+)*fn\\s+(\\w+)"));
    defs.insert(QStringLiteral("Rust"), rust);

    LanguageDefinition swift;
    swift.comments = CommentStyle::CLike;
    swift.rules << braceContainer(QStringLiteral("\\b(?:class|struct|enum|protocol|extension|actor)\\s+([A-Za-z_]\\w*)"))
                << rule(QStringLiteral("\\bfunc\\s+([^\\s(<]+)"))
                << rule(QStringLiteral("^\\s*(?:(?:public|private|internal|fileprivate|open|convenience|required|override)\\s+)*(init|deinit)\\b"));
    defs.insert(QStringLiteral("Swift"), swift);

    LanguageDefinition python;
    python.comments = CommentStyle::Python;
    python.strategy = Strategy::Indentation;
    python.rules << rule(QStringLiteral("^([ \\t]*)class\\s+(\\w+)"), 2, true)
                 << rule(QStringLiteral("^([ \\t]*)(?:async\\s+)?def\\s+(\\w+)"), 2);
    defs.insert(QStringLiteral("Python"), python);

    LanguageDefinition ruby;
    ruby.comments = CommentStyle::Hash;
    ruby.strategy = Strategy::Indentation;
    ruby.rules << rule(QStringLiteral("^([ \\t]*)(?:class|module)\\s+([\\w:]+)"), 2, true)
               << rule(QStringLiteral("^([ \\t]*)def\\s+((?:self\\.)?[\\w\\.\\?!=\\[\\]]+)"), 2);
    defs.insert(QStringLiteral("Ruby"), ruby);

    LanguageDefinition coffee;
    coffee.comments = CommentStyle::Hash;
    coffee.strategy = Strategy::Indentation;
    coffee.rules << rule(QStringLiteral("^([ \\t]*)class\\s+([\\w\\.@]+)"), 2, true)
                 << rule(QStringLiteral("^([ \\t]*)([\\w\\.@]+)\\s*[:=]\\s*(?:\\([^)]*\\)\\s*)?[-=]>"), 2);
    defs.insert(QStringLiteral("CoffeeScript"), coffee);

    LanguageDefinition nim;
    nim.comments = CommentStyle::Hash;
    nim.strategy = Strategy::Indentation;
    nim.rules << rule(QStringLiteral("^([ \\t]*)type\\s+(\\w+)"), 2, true)
              << rule(QStringLiteral("^([ \\t]*)(?:proc|func|method|iterator|template|macro|converter)\\s+`?(\\w+)"), 2);
    defs.insert(QStringLiteral("Nimrod"), nim);

    LanguageDefinition lua;
    lua.comments = CommentStyle::Lua;
    lua.qualifierSeparator = QStringLiteral(".");
    lua.rules << rule(QStringLiteral("^\\s*(?:local\\s+)?function\\s+([\\w\\.:]+)"))
              << rule(QStringLiteral("^\\s*(?:local\\s+)?([\\w\\.:]+)\\s*=\\s*function\\b"));
    defs.insert(QStringLiteral("Lua"), lua);

    LanguageDefinition perl;
    perl.comments = CommentStyle::Hash;
    perl.rules << rule(QStringLiteral("^\\s*package\\s+([\\w:]+)"), 1, true)
               << rule(QStringLiteral("^\\s*sub\\s+([\\w:]+)"));
    defs.insert(QStringLiteral("Perl"), perl);

    LanguageDefinition bash;
    bash.comments = CommentStyle::Hash;
    bash.rules << rule(QStringLiteral("^\\s*function\\s+([\\w\\.\\-:]+)"))
               << rule(QStringLiteral("^\\s*([\\w\\.\\-:]+)\\s*\\(\\s*\\)"));
    defs.insert(QStringLiteral("bash"), bash);

    LanguageDefinition powershell;
    powershell.comments = CommentStyle::Hash;
    powershell.rules << rule(QStringLiteral("^\\s*class\\s+([\\w\\-]+)"), 1, true, true)
                     << rule(QStringLiteral("^\\s*(?:function|filter|workflow)\\s+([\\w\\-:]+)"), 1, false, true);
    defs.insert(QStringLiteral("PowerShell"), powershell);

    LanguageDefinition tcl;
    tcl.comments = CommentStyle::Hash;
    tcl.rules << rule(QStringLiteral("^\\s*proc\\s+([\\w:]+)"));
    defs.insert(QStringLiteral("TCL"), tcl);

    LanguageDefinition r;
    r.comments = CommentStyle::Hash;
    r.rules << rule(QStringLiteral("^\\s*([\\w\\.]+)\\s*(?:<-|=)\\s*function\\b"));
    defs.insert(QStringLiteral("R"), r);

    LanguageDefinition cmake;
    cmake.comments = CommentStyle::Hash;
    cmake.rules << rule(QStringLiteral("^\\s*(?:function|macro)\\s*\\(\\s*([\\w\\-]+)"), 1, false, true);
    defs.insert(QStringLiteral("CMakeFile"), cmake);

    LanguageDefinition makefile;
    makefile.comments = CommentStyle::Hash;
    makefile.rules << rule(QStringLiteral("^([A-Za-z0-9_\\.\\-/\\$\\(\\)%][A-Za-z0-9_\\.\\-/\\$\\(\\)% ]*?)\\s*::?(?!=)"));
    defs.insert(QStringLiteral("Makefile"), makefile);

    LanguageDefinition yaml;
    yaml.comments = CommentStyle::Hash;
    yaml.rules << rule(QStringLiteral("^([A-Za-z_\"'][^:#\\n]*?)\\s*:(?:\\s|$)"));
    defs.insert(QStringLiteral("YAML"), yaml);

    LanguageDefinition sql;
    sql.comments = CommentStyle::Sql;
    sql.rules << rule(QStringLiteral("\\bcreate\\s+(?:or\\s+(?:replace|alter)\\s+)?(?:temporary\\s+|temp\\s+)?(?:procedure|proc|function|view|trigger|table|package(?:\\s+body)?|type)\\s+(?:if\\s+not\\s+exists\\s+)?([\\w\\.\\[\\]\"`]+)"), 1, false, true);
    defs.insert(QStringLiteral("SQL"), sql);

    LanguageDefinition pascal;
    pascal.comments = CommentStyle::Pascal;
    pascal.qualifierSeparator = QStringLiteral(".");
    pascal.rules << rule(QStringLiteral("^\\s*(?:class\\s+)?(?:procedure|function|constructor|destructor|operator)\\s+([\\w\\.]+)"), 1, false, true);
    defs.insert(QStringLiteral("Pascal"), pascal);

    LanguageDefinition vb;
    vb.comments = CommentStyle::VisualBasic;
    vb.rules << rule(QStringLiteral("^\\s*(?:(?:public|private|friend|protected|partial)\\s+)*(?:class|module|structure|interface)\\s+(\\w+)"), 1, true, true)
             << rule(QStringLiteral("^\\s*(?:(?:public|private|friend|protected|static|shared|overrides|overridable|overloads|async|iterator)\\s+)*(?:sub|function|property\\s+(?:get|let|set))\\s+(\\w+)"), 1, false, true);
    defs.insert(QStringLiteral("VB / VBS"), vb);

    LanguageDefinition batch;
    batch.rules << rule(QStringLiteral("^\\s*:([^\\s:][^\\s]*)"));
    defs.insert(QStringLiteral("Batch"), batch);

    LanguageDefinition ini;
    ini.rules << rule(QStringLiteral("^\\s*\\[([^\\]\\r\\n]+)\\]"));
    defs.insert(QStringLiteral("ini file"), ini);
    defs.insert(QStringLiteral("Properties file"), ini);
    defs.insert(QStringLiteral("registry"), ini);
    defs.insert(QStringLiteral("InnoSetup"), ini);

    LanguageDefinition matlab;
    matlab.comments = CommentStyle::Percent;
    matlab.rules << rule(QStringLiteral("^\\s*function\\s+(?:\\[?[\\w,\\s]*\\]?\\s*=\\s*)?(\\w+)"));
    defs.insert(QStringLiteral("Matlab"), matlab);

    LanguageDefinition lisp;
    lisp.comments = CommentStyle::Semicolon;
    lisp.rules << rule(QStringLiteral("\\(\\s*(?:define|defun|defmacro|defn|defgeneric|defmethod)\\s+\\(?([^\\s()]+)"), 1, false, true);
    defs.insert(QStringLiteral("LISP"), lisp);
    defs.insert(QStringLiteral("Scheme"), lisp);

    LanguageDefinition asmDef;
    asmDef.comments = CommentStyle::Semicolon;
    asmDef.rules << rule(QStringLiteral("^([A-Za-z_\\.][\\w\\.]*):"));
    defs.insert(QStringLiteral("Assembly"), asmDef);

    LanguageDefinition autoit;
    autoit.comments = CommentStyle::Semicolon;
    autoit.rules << rule(QStringLiteral("^\\s*Func\\s+(\\w+)"), 1, false, true);
    defs.insert(QStringLiteral("autoIt"), autoit);

    LanguageDefinition nsis;
    nsis.comments = CommentStyle::Semicolon;
    nsis.rules << rule(QStringLiteral("^\\s*(?:Function|Section)\\s+(?:/o\\s+)?\"?([^\"\\r\\n]+?)\"?\\s*$"), 1, false, true);
    defs.insert(QStringLiteral("NSIS"), nsis);

    LanguageDefinition fortran;
    fortran.rules << rule(QStringLiteral("^\\s*(?:(?:recursive|pure|elemental|impure|module)\\s+)*(?:\\w+(?:\\([^)]*\\))?\\s+)?(?:subroutine|function|program)\\s+(\\w+)"), 1, false, true)
                  << rule(QStringLiteral("^\\s*module\\s+(?!procedure)(\\w+)"), 1, true, true);
    defs.insert(QStringLiteral("Fortran (free form)"), fortran);
    defs.insert(QStringLiteral("Fortran (fixed form)"), fortran);

    LanguageDefinition haskell;
    haskell.rules << rule(QStringLiteral("^([a-z_][\\w']*)\\s*::"));
    defs.insert(QStringLiteral("Haskell"), haskell);

    LanguageDefinition erlang;
    erlang.comments = CommentStyle::Percent;
    erlang.rules << rule(QStringLiteral("^([a-z]\\w*)\\s*\\("));
    defs.insert(QStringLiteral("Erlang"), erlang);

    LanguageDefinition verilog;
    verilog.comments = CommentStyle::CLike;
    verilog.rules << rule(QStringLiteral("^\\s*(?:module|task|function|class|interface|package)\\s+(?:automatic\\s+)?(?:\\w+\\s+)?(\\w+)\\s*[;(#]"));
    defs.insert(QStringLiteral("Verilog"), verilog);

    LanguageDefinition vhdl;
    vhdl.rules << rule(QStringLiteral("^\\s*(?:entity|architecture|package(?:\\s+body)?|procedure|function)\\s+(\\w+)"), 1, false, true);
    defs.insert(QStringLiteral("VHDL"), vhdl);

    LanguageDefinition markdown;
    markdown.strategy = Strategy::Markdown;
    Rule heading = rule(QStringLiteral("^(#{1,6})[ \\t]+(.+?)[ \\t]*#*[ \\t]*$"), 2);
    heading.levelGroup = 1;
    markdown.rules << heading;
    defs.insert(QStringLiteral("Markdown"), markdown);
    defs.insert(QStringLiteral("txt2tags"), markdown);

    LanguageDefinition latex;
    latex.comments = CommentStyle::Percent;
    latex.strategy = Strategy::Markdown;
    Rule section = rule(QStringLiteral("\\\\(part|chapter|section|subsection|subsubsection|paragraph)\\*?\\s*(?:\\[[^\\]]*\\])?\\{([^}]*)\\}"), 2);
    section.levelGroup = 1;
    latex.rules << section;
    defs.insert(QStringLiteral("LaTeX"), latex);
    defs.insert(QStringLiteral("TeX"), latex);

    return defs;
}

// Replaces comments and string literals with spaces (keeping new lines) so the text can be
// searched without false positives while keeping all positions the same.
QString stripCommentsAndStrings(const QString &text, CommentStyle style, bool preprocessor)
{
    if (style == CommentStyle::None && !preprocessor) {
        return text;
    }

    QString result = text;
    const int length = text.size();

    auto blank = [&](int from, int to) {
        for (int i = from; i < to && i < length; ++i) {
            if (result[i] != QLatin1Char('\n') && result[i] != QLatin1Char('\r')) {
                result[i] = QLatin1Char(' ');
            }
        }
    };

    auto skipToEndOfLine = [&](int i) {
        while (i < length && text[i] != QLatin1Char('\n')) i++;
        return i;
    };

    auto skipString = [&](int i, QChar quote, bool escapes) {
        // i is at the opening quote
        int j = i + 1;
        while (j < length) {
            if (escapes && text[j] == QLatin1Char('\\')) {
                j += 2;
                continue;
            }
            if (text[j] == quote) {
                return j + 1;
            }
            if (text[j] == QLatin1Char('\n') && quote != QLatin1Char('`')) {
                return j; // unterminated
            }
            j++;
        }
        return length;
    };

    auto skipBlock = [&](int i, const QString &end) {
        const int found = text.indexOf(end, i);
        return found == -1 ? length : found + static_cast<int>(end.size());
    };

    bool atLineStart = true;
    int i = 0;

    while (i < length) {
        const QChar c = text[i];
        const QChar next = i + 1 < length ? text[i + 1] : QChar();
        int end = -1;
        bool keepDelimiters = false;

        if (preprocessor && atLineStart && c == QLatin1Char('#')) {
            // Preprocessor directive, including continuation lines
            end = i;
            while (end < length && text[end] != QLatin1Char('\n')) {
                if (text[end] == QLatin1Char('\\') && end + 1 < length && text[end + 1] == QLatin1Char('\n')) {
                    end += 2;
                    continue;
                }
                end++;
            }
        }
        else {
            switch (style) {
            case CommentStyle::CLike:
            case CommentStyle::JavaScript:
                if (c == QLatin1Char('/') && next == QLatin1Char('/')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('/') && next == QLatin1Char('*')) end = skipBlock(i + 2, QStringLiteral("*/"));
                else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) { end = skipString(i, c, true); keepDelimiters = true; }
                else if (style == CommentStyle::JavaScript && c == QLatin1Char('`')) { end = skipString(i, c, true); keepDelimiters = true; }
                break;
            case CommentStyle::Hash:
                if (c == QLatin1Char('#')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) { end = skipString(i, c, true); keepDelimiters = true; }
                break;
            case CommentStyle::Python:
                if (c == QLatin1Char('#')) end = skipToEndOfLine(i);
                else if ((c == QLatin1Char('"') || c == QLatin1Char('\'')) && text.mid(i, 3) == QString(3, c)) end = skipBlock(i + 3, QString(3, c));
                else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) { end = skipString(i, c, true); keepDelimiters = true; }
                break;
            case CommentStyle::Lua:
                if (c == QLatin1Char('-') && next == QLatin1Char('-')) {
                    if (text.mid(i + 2, 2) == QLatin1String("[[")) end = skipBlock(i + 4, QStringLiteral("]]"));
                    else end = skipToEndOfLine(i);
                }
                else if (c == QLatin1Char('[') && next == QLatin1Char('[')) end = skipBlock(i + 2, QStringLiteral("]]"));
                else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) { end = skipString(i, c, true); keepDelimiters = true; }
                break;
            case CommentStyle::Sql:
                if (c == QLatin1Char('-') && next == QLatin1Char('-')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('/') && next == QLatin1Char('*')) end = skipBlock(i + 2, QStringLiteral("*/"));
                else if (c == QLatin1Char('\'')) { end = skipString(i, c, false); keepDelimiters = true; }
                break;
            case CommentStyle::Pascal:
                if (c == QLatin1Char('/') && next == QLatin1Char('/')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('{')) end = skipBlock(i + 1, QStringLiteral("}"));
                else if (c == QLatin1Char('(') && next == QLatin1Char('*')) end = skipBlock(i + 2, QStringLiteral("*)"));
                else if (c == QLatin1Char('\'')) { end = skipString(i, c, false); keepDelimiters = true; }
                break;
            case CommentStyle::VisualBasic:
                if (c == QLatin1Char('\'')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('"')) { end = skipString(i, c, false); keepDelimiters = true; }
                break;
            case CommentStyle::Percent:
                if (c == QLatin1Char('%')) end = skipToEndOfLine(i);
                break;
            case CommentStyle::Semicolon:
                if (c == QLatin1Char(';')) end = skipToEndOfLine(i);
                else if (c == QLatin1Char('"')) { end = skipString(i, c, false); keepDelimiters = true; }
                break;
            case CommentStyle::None:
                break;
            }
        }

        if (end > i) {
            if (keepDelimiters) {
                blank(i + 1, end - 1);
            }
            else {
                blank(i, end);
            }
            i = end;
            atLineStart = false;
            continue;
        }

        if (c == QLatin1Char('\n')) {
            atLineStart = true;
        }
        else if (!c.isSpace()) {
            atLineStart = false;
        }

        i++;
    }

    return result;
}

class LineIndex
{
public:
    explicit LineIndex(const QString &text)
    {
        starts.append(0);
        for (int i = 0; i < text.size(); ++i) {
            if (text[i] == QLatin1Char('\n')) {
                starts.append(i + 1);
            }
        }
    }

    int lineFromPosition(int pos) const
    {
        auto it = std::upper_bound(starts.begin(), starts.end(), pos);
        return static_cast<int>(it - starts.begin()) - 1;
    }

private:
    QVector<int> starts;
};

int matchingBrace(const QString &text, int openPos)
{
    int depth = 0;

    for (int i = openPos; i < text.size(); ++i) {
        const QChar c = text[i];

        if (c == QLatin1Char('{')) {
            depth++;
        }
        else if (c == QLatin1Char('}')) {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }

    return text.size();
}

int matchingParen(const QString &text, int openPos)
{
    int depth = 0;

    for (int i = openPos; i < text.size() && i < openPos + 5000; ++i) {
        const QChar c = text[i];

        if (c == QLatin1Char('(')) {
            depth++;
        }
        else if (c == QLatin1Char(')')) {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
        else if (c == QLatin1Char('{') || c == QLatin1Char('}') || c == QLatin1Char(';')) {
            return -1;
        }
    }

    return -1;
}

bool isIdentifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

struct Container {
    QString name;
    int start; // position of the opening brace
    int end;   // position of the closing brace
};

// Finds where the body of a function definition starts, or -1 if it is not a definition
int findFunctionBody(const QString &text, int closeParen)
{
    static const QSet<QString> qualifiers = {
        QStringLiteral("const"), QStringLiteral("noexcept"), QStringLiteral("override"), QStringLiteral("final"),
        QStringLiteral("volatile"), QStringLiteral("mutable"), QStringLiteral("throw"), QStringLiteral("throws"),
        QStringLiteral("try"), QStringLiteral("where"), QStringLiteral("async"), QStringLiteral("sealed"),
        QStringLiteral("nothrow"), QStringLiteral("pure"), QStringLiteral("@safe"), QStringLiteral("@trusted"),
        QStringLiteral("use"), QStringLiteral("Q_DECL_OVERRIDE"), QStringLiteral("Q_DECL_NOEXCEPT"),
    };

    const int length = text.size();
    int p = closeParen + 1;
    const int limit = qMin(length, closeParen + 2000);

    while (p < limit) {
        const QChar c = text[p];

        if (c.isSpace()) {
            p++;
        }
        else if (c == QLatin1Char('{')) {
            return p;
        }
        else if (isIdentifierChar(c) || c == QLatin1Char('@')) {
            const int wordStart = p;
            while (p < limit && (isIdentifierChar(text[p]) || text[p] == QLatin1Char('@'))) p++;
            const QString word = text.mid(wordStart, p - wordStart);

            if (!qualifiers.contains(word)) {
                return -1;
            }

            if (word == QLatin1String("throws")) {
                // Java: throws A, B
                while (p < limit && (isIdentifierChar(text[p]) || text[p].isSpace() || text[p] == QLatin1Char(',') || text[p] == QLatin1Char('.') || text[p] == QLatin1Char('<') || text[p] == QLatin1Char('>'))) p++;
            }
            else if (word == QLatin1String("use")) {
                // PHP closures: function () use ($x)
                while (p < limit && text[p].isSpace()) p++;
                if (p < limit && text[p] == QLatin1Char('(')) {
                    const int close = matchingParen(text, p);
                    if (close == -1) return -1;
                    p = close + 1;
                }
            }
            else {
                // e.g. noexcept(...) or throw(...)
                int q = p;
                while (q < limit && text[q].isSpace()) q++;
                if (q < limit && text[q] == QLatin1Char('(') && (word == QLatin1String("noexcept") || word == QLatin1String("throw"))) {
                    const int close = matchingParen(text, q);
                    if (close == -1) return -1;
                    p = close + 1;
                }
            }
        }
        else if (c == QLatin1Char('&')) {
            p++;
        }
        else if (c == QLatin1Char('-') && p + 1 < limit && text[p + 1] == QLatin1Char('>')) {
            // Trailing return type
            p += 2;
            while (p < limit && text[p] != QLatin1Char('{') && text[p] != QLatin1Char(';') && text[p] != QLatin1Char('=') && text[p] != QLatin1Char('}')) {
                if (text[p] == QLatin1Char('(')) {
                    const int close = matchingParen(text, p);
                    if (close == -1) return -1;
                    p = close + 1;
                }
                else {
                    p++;
                }
            }
        }
        else if (c == QLatin1Char(':') && !(p + 1 < limit && text[p + 1] == QLatin1Char(':'))) {
            // Constructor initializer list, or a type annotation (TypeScript, PHP)
            p++;
            while (p < limit && text[p] != QLatin1Char('{') && text[p] != QLatin1Char(';') && text[p] != QLatin1Char('}') && text[p] != QLatin1Char('=')) {
                if (text[p] == QLatin1Char('(')) {
                    const int close = matchingParen(text, p);
                    if (close == -1) return -1;
                    p = close + 1;
                }
                else {
                    p++;
                }
            }
        }
        else if (c == QLatin1Char('[') && p + 1 < limit && text[p + 1] == QLatin1Char('[')) {
            const int end = text.indexOf(QLatin1String("]]"), p);
            if (end == -1) return -1;
            p = end + 2;
        }
        else {
            return -1;
        }
    }

    return -1;
}

void parseCLike(const QString &text, const LanguageDefinition &definition, QVector<Entry> &entries)
{
    static const QSet<QString> nonFunctionNames = {
        QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("foreach"), QStringLiteral("while"), QStringLiteral("switch"),
        QStringLiteral("catch"), QStringLiteral("return"), QStringLiteral("sizeof"), QStringLiteral("typeof"), QStringLiteral("nameof"),
        QStringLiteral("alignof"), QStringLiteral("decltype"), QStringLiteral("using"), QStringLiteral("lock"), QStringLiteral("fixed"),
        QStringLiteral("synchronized"), QStringLiteral("function"), QStringLiteral("elseif"), QStringLiteral("else"), QStringLiteral("do"),
        QStringLiteral("with"), QStringLiteral("defined"), QStringLiteral("static_assert"), QStringLiteral("new"), QStringLiteral("delete"),
        QStringLiteral("throw"), QStringLiteral("await"), QStringLiteral("yield"), QStringLiteral("assert"), QStringLiteral("unless"),
        QStringLiteral("try"), QStringLiteral("finally"), QStringLiteral("checked"), QStringLiteral("unchecked"), QStringLiteral("unsafe"),
        QStringLiteral("operator"), QStringLiteral("constexpr"),
        QStringLiteral("elif"), QStringLiteral("SIGNAL"), QStringLiteral("SLOT"),
    };

    static const QSet<QString> rejectedPrecedingWords = {
        QStringLiteral("return"), QStringLiteral("new"), QStringLiteral("else"), QStringLiteral("throw"), QStringLiteral("case"),
        QStringLiteral("await"), QStringLiteral("yield"), QStringLiteral("typeof"), QStringLiteral("delete"), QStringLiteral("goto"),
        QStringLiteral("do"), QStringLiteral("in"), QStringLiteral("of"), QStringLiteral("instanceof"), QStringLiteral("sizeof"),
        QStringLiteral("echo"), QStringLiteral("print"), QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"),
        QStringLiteral("if"), QStringLiteral("while"), QStringLiteral("for"),
    };

    const LineIndex lines(text);

    // Find the containers (classes, structs, ...) first
    QVector<Container> containers;
    for (const Rule &r : definition.rules) {
        if (!r.isContainer) {
            continue;
        }

        QRegularExpressionMatchIterator it = r.regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString name = match.captured(r.nameGroup);

            if (name.isEmpty()) {
                continue;
            }

            if (r.braceBody) {
                // The body starts at the next brace, as long as it is not just a declaration
                int p = match.capturedEnd();
                while (p < text.size() && text[p] != QLatin1Char('{') && text[p] != QLatin1Char(';') && text[p] != QLatin1Char('(') && text[p] != QLatin1Char(')') && text[p] != QLatin1Char('=')) {
                    p++;
                }

                if (p >= text.size() || text[p] != QLatin1Char('{')) {
                    continue;
                }

                containers.append({name, p, matchingBrace(text, p)});
            }
            else {
                // Not delimited by braces (e.g. Objective-C's @implementation), it lasts until the next container
                containers.append({name, static_cast<int>(match.capturedStart()), -1});
            }

            Entry entry;
            entry.name = name;
            entry.line = lines.lineFromPosition(match.capturedStart(r.nameGroup));
            entry.isContainer = true;
            entries.append(entry);
        }
    }

    // Containers without braces end where the next one starts
    std::sort(containers.begin(), containers.end(), [](const Container &a, const Container &b) { return a.start < b.start; });
    for (int i = 0; i < containers.size(); ++i) {
        if (containers[i].end == -1) {
            containers[i].end = text.size();
            for (int j = i + 1; j < containers.size(); ++j) {
                if (containers[j].end == -1 || containers[j].start > containers[i].start) {
                    containers[i].end = containers[j].start;
                    break;
                }
            }
        }
    }

    auto containerAt = [&](int pos) -> QString {
        QString best;
        int bestSize = INT_MAX;

        for (const Container &container : containers) {
            if (pos > container.start && pos < container.end && (container.end - container.start) < bestSize) {
                best = container.name;
                bestSize = container.end - container.start;
            }
        }

        return best;
    };

    // Functions
    static const QRegularExpression functionName(QStringLiteral("([A-Za-z_~$][\\w$]*(?:\\s*::\\s*~?[A-Za-z_][\\w]*)*)\\s*\\("));
    QRegularExpressionMatchIterator it = functionName.globalMatch(text);
    int insideFunctionUntil = -1;

    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int nameStart = match.capturedStart(1);

        if (nameStart < insideFunctionUntil) {
            continue;
        }

        QString fullName = match.captured(1);
        fullName.remove(QRegularExpression(QStringLiteral("\\s+")));

        const int separator = fullName.lastIndexOf(QLatin1String("::"));
        const QString shortName = separator >= 0 ? fullName.mid(separator + 2) : fullName;

        if (nonFunctionNames.contains(shortName)) {
            continue;
        }

        // Check what comes before the name
        int q = nameStart - 1;
        while (q >= 0 && text[q].isSpace()) q--;

        if (q >= 0) {
            const QChar prev = text[q];

            if (isIdentifierChar(prev)) {
                int wordEnd = q + 1;
                while (q >= 0 && isIdentifierChar(text[q])) q--;
                const QString word = text.mid(q + 1, wordEnd - q - 1);

                if (rejectedPrecedingWords.contains(word)) {
                    continue;
                }
            }
            else if (!(prev == QLatin1Char('*') || prev == QLatin1Char('&') || prev == QLatin1Char('>') || prev == QLatin1Char(']') ||
                       prev == QLatin1Char(':') || prev == QLatin1Char('}') || prev == QLatin1Char(';') || prev == QLatin1Char('{') ||
                       prev == QLatin1Char(')') || prev == QLatin1Char('@'))) {
                continue;
            }

            // Make sure this is not a call through a member access
            if (prev == QLatin1Char('>') && q > 0 && text[q - 1] == QLatin1Char('-')) {
                continue;
            }
        }

        const int openParen = match.capturedEnd() - 1;
        const int closeParen = matchingParen(text, openParen);

        if (closeParen == -1) {
            continue;
        }

        const int bodyStart = findFunctionBody(text, closeParen);
        if (bodyStart == -1) {
            continue;
        }

        Entry entry;
        entry.name = shortName;
        entry.line = lines.lineFromPosition(nameStart);

        if (separator >= 0) {
            entry.container = fullName.left(separator);
        }
        else {
            entry.container = containerAt(nameStart);
        }

        entries.append(entry);

        insideFunctionUntil = matchingBrace(text, bodyStart);
    }
}

void parseRules(const QString &text, const LanguageDefinition &definition, QVector<Entry> &entries)
{
    const LineIndex lines(text);
    QVector<Container> containers;

    // Containers first so functions can be put inside of them
    for (const Rule &r : definition.rules) {
        if (!r.isContainer) {
            continue;
        }

        QRegularExpressionMatchIterator it = r.regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString name = match.captured(r.nameGroup).trimmed();

            if (name.isEmpty()) {
                continue;
            }

            int start = match.capturedStart();
            int end = -1;

            if (r.braceBody) {
                int p = match.capturedEnd();
                while (p < text.size() && text[p] != QLatin1Char('{') && text[p] != QLatin1Char(';')) p++;

                if (p >= text.size() || text[p] != QLatin1Char('{')) {
                    continue;
                }

                start = p;
                end = matchingBrace(text, p);
            }

            containers.append({name, start, end});

            Entry entry;
            entry.name = name;
            entry.line = lines.lineFromPosition(match.capturedStart(r.nameGroup));
            entry.isContainer = true;
            entries.append(entry);
        }
    }

    std::sort(containers.begin(), containers.end(), [](const Container &a, const Container &b) { return a.start < b.start; });
    for (int i = 0; i < containers.size(); ++i) {
        if (containers[i].end == -1) {
            containers[i].end = (i + 1 < containers.size()) ? containers[i + 1].start : text.size();
        }
    }

    for (const Rule &r : definition.rules) {
        if (r.isContainer) {
            continue;
        }

        QRegularExpressionMatchIterator it = r.regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            QString name = match.captured(r.nameGroup).trimmed();

            if (name.isEmpty()) {
                continue;
            }

            Entry entry;
            entry.line = lines.lineFromPosition(match.capturedStart(r.nameGroup));

            // Qualified names like Lua's "M.b" / "M:c" or Pascal's "TClass.Method"
            int separator = -1;
            QString normalized = name;
            if (!definition.qualifierSeparator.isEmpty()) {
                if (definition.qualifierSeparator == QLatin1String(".")) {
                    normalized.replace(QLatin1Char(':'), QLatin1Char('.'));
                }
                separator = normalized.lastIndexOf(definition.qualifierSeparator);
            }

            if (r.containerGroup >= 0 && !match.captured(r.containerGroup).isEmpty()) {
                entry.container = match.captured(r.containerGroup);
            }
            else if (separator > 0) {
                entry.container = normalized.left(separator);
                name = name.mid(separator + definition.qualifierSeparator.size());
            }
            else {
                const int pos = match.capturedStart(r.nameGroup);
                int bestSize = INT_MAX;
                for (const Container &container : containers) {
                    if (pos > container.start && pos < container.end && (container.end - container.start) < bestSize) {
                        entry.container = container.name;
                        bestSize = container.end - container.start;
                    }
                }
            }

            entry.name = name;
            entries.append(entry);
        }
    }
}

int indentationWidth(const QString &indent)
{
    int width = 0;
    for (const QChar c : indent) {
        width += (c == QLatin1Char('\t')) ? 8 - (width % 8) : 1;
    }
    return width;
}

void parseIndentation(const QString &text, const LanguageDefinition &definition, QVector<Entry> &entries)
{
    struct Match {
        int pos;
        int indent;
        QString name;
        bool isContainer;
    };

    QVector<Match> matches;
    for (const Rule &r : definition.rules) {
        QRegularExpressionMatchIterator it = r.regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            matches.append({static_cast<int>(match.capturedStart()), indentationWidth(match.captured(1)), match.captured(r.nameGroup), r.isContainer});
        }
    }

    std::sort(matches.begin(), matches.end(), [](const Match &a, const Match &b) { return a.pos < b.pos; });

    const LineIndex lines(text);
    QVector<Match> stack;

    for (const Match &m : matches) {
        while (!stack.isEmpty() && stack.last().indent >= m.indent) {
            stack.removeLast();
        }

        // Skip functions nested in other functions
        if (!m.isContainer && !stack.isEmpty() && !stack.last().isContainer) {
            continue;
        }

        Entry entry;
        entry.name = m.name;
        entry.line = lines.lineFromPosition(m.pos);
        entry.isContainer = m.isContainer;

        for (int i = stack.size() - 1; i >= 0; --i) {
            if (stack[i].isContainer) {
                entry.container = stack[i].name;
                break;
            }
        }

        // Nested classes are shown with their full name
        if (m.isContainer && !entry.container.isEmpty()) {
            entry.name = entry.container + QLatin1Char('.') + entry.name;
            entry.container.clear();
        }

        entries.append(entry);
        stack.append(m.isContainer ? Match{m.pos, m.indent, entry.name, true} : m);
    }
}

void parseHeadings(const QString &text, const LanguageDefinition &definition, QVector<Entry> &entries)
{
    static const QHash<QString, int> latexLevels = {
        {QStringLiteral("part"), 1}, {QStringLiteral("chapter"), 2}, {QStringLiteral("section"), 3},
        {QStringLiteral("subsection"), 4}, {QStringLiteral("subsubsection"), 5}, {QStringLiteral("paragraph"), 6},
    };

    const LineIndex lines(text);

    for (const Rule &r : definition.rules) {
        QRegularExpressionMatchIterator it = r.regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString levelText = match.captured(r.levelGroup);

            Entry entry;
            entry.name = match.captured(r.nameGroup).trimmed();
            entry.line = lines.lineFromPosition(match.capturedStart());
            entry.level = latexLevels.value(levelText, levelText.size());
            entry.isContainer = true;
            entries.append(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.line < b.line; });
}

} // namespace

bool supportsLanguage(const QString &languageName)
{
    return definitions().contains(languageName);
}

QVector<Entry> parse(const QString &languageName, const QString &text)
{
    QVector<Entry> entries;

    const auto it = definitions().constFind(languageName);
    if (it == definitions().constEnd()) {
        return entries;
    }

    const LanguageDefinition &definition = it.value();

    // Markdown code blocks should not be treated as headings
    QString cleaned;
    if (definition.strategy == Strategy::Markdown && definition.comments == CommentStyle::None) {
        cleaned = text;
        static const QRegularExpression fence(QStringLiteral("^(```|~~~)[^\\n]*\\n.*?^\\1"), QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator fences = fence.globalMatch(text);
        while (fences.hasNext()) {
            const QRegularExpressionMatch match = fences.next();
            for (int i = match.capturedStart(); i < match.capturedEnd(); ++i) {
                if (cleaned[i] != QLatin1Char('\n')) cleaned[i] = QLatin1Char(' ');
            }
        }
    }
    else {
        cleaned = stripCommentsAndStrings(text, definition.comments, definition.preprocessor);
    }

    switch (definition.strategy) {
    case Strategy::CLikeScanner:
        parseCLike(cleaned, definition, entries);
        break;
    case Strategy::Rules:
        parseRules(cleaned, definition, entries);
        break;
    case Strategy::Indentation:
        parseIndentation(cleaned, definition, entries);
        break;
    case Strategy::Markdown:
        parseHeadings(cleaned, definition, entries);
        break;
    }

    // Some rules (e.g. JavaScript arrow functions) may find things the scanner already found
    if (definition.strategy == Strategy::CLikeScanner) {
        QVector<Entry> withRules;
        LanguageDefinition rulesOnly = definition;
        rulesOnly.rules.clear();
        for (const Rule &r : definition.rules) {
            if (!r.isContainer) {
                rulesOnly.rules.append(r);
            }
        }

        if (!rulesOnly.rules.isEmpty()) {
            parseRules(cleaned, rulesOnly, withRules);
            for (const Entry &e : withRules) {
                const bool duplicate = std::any_of(entries.begin(), entries.end(), [&](const Entry &existing) {
                    return existing.line == e.line && existing.name == e.name;
                });

                if (!duplicate) {
                    entries.append(e);
                }
            }
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.line < b.line; });

    return entries;
}

}
