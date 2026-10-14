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

#include "FindInFilesDialog.h"

#include "ApplicationSettings.h"
#include "ISearchResultsHandler.h"
#include "MainWindow.h"
#include "NotepadNextApplication.h"
#include "TextEncoding.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QVBoxLayout>


static const qint64 MAX_FILE_SIZE = 100 * 1024 * 1024;

static ApplicationSettings *settings()
{
    return qobject_cast<NotepadNextApplication *>(qApp)->getSettings();
}

static QComboBox *createCombo()
{
    QComboBox *combo = new QComboBox();
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setCompleter(nullptr);
    combo->setMinimumWidth(320);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return combo;
}

FindInFilesDialog::FindInFilesDialog(MainWindow *window) :
    QDialog(window, Qt::Dialog),
    window(window)
{
    setWindowTitle(tr("Find in Files"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    comboFind = createCombo();
    comboReplace = createCombo();
    comboFilters = createCombo();
    comboDirectory = createCombo();

    QPushButton *buttonBrowse = new QPushButton(QStringLiteral("..."));
    buttonBrowse->setMaximumWidth(40);
    connect(buttonBrowse, &QPushButton::clicked, this, &FindInFilesDialog::browseDirectory);

    QHBoxLayout *directoryLayout = new QHBoxLayout();
    directoryLayout->addWidget(comboDirectory);
    directoryLayout->addWidget(buttonBrowse);

    QFormLayout *form = new QFormLayout();
    form->addRow(tr("Find what:"), comboFind);
    form->addRow(tr("Replace with:"), comboReplace);
    form->addRow(tr("Filters:"), comboFilters);
    form->addRow(tr("Directory:"), directoryLayout);

    checkFollowCurrentDoc = new QCheckBox(tr("Follow current doc."));
    checkSubFolders = new QCheckBox(tr("In all sub-folders"));
    checkHiddenFolders = new QCheckBox(tr("In hidden folders"));
    checkWholeWord = new QCheckBox(tr("Match &whole word only"));
    checkMatchCase = new QCheckBox(tr("Match &case"));

    radioNormal = new QRadioButton(tr("&Normal"));
    radioExtended = new QRadioButton(tr("E&xtended (\\n, \\r, \\t, \\0, \\x...)"));
    radioRegex = new QRadioButton(tr("Re&gular expression"));
    checkDotMatchesNewline = new QCheckBox(tr(". matches newline"));
    checkDotMatchesNewline->setEnabled(false);
    connect(radioRegex, &QRadioButton::toggled, checkDotMatchesNewline, &QCheckBox::setEnabled);
    connect(radioRegex, &QRadioButton::toggled, checkWholeWord, [=](bool regex) { checkWholeWord->setEnabled(!regex); });

    QGroupBox *searchMode = new QGroupBox(tr("Search Mode"));
    QVBoxLayout *searchModeLayout = new QVBoxLayout(searchMode);
    searchModeLayout->addWidget(radioNormal);
    searchModeLayout->addWidget(radioExtended);
    QHBoxLayout *regexLayout = new QHBoxLayout();
    regexLayout->addWidget(radioRegex);
    regexLayout->addWidget(checkDotMatchesNewline);
    regexLayout->addStretch();
    searchModeLayout->addLayout(regexLayout);

    QVBoxLayout *optionsLayout = new QVBoxLayout();
    optionsLayout->addWidget(checkWholeWord);
    optionsLayout->addWidget(checkMatchCase);
    optionsLayout->addStretch();

    QVBoxLayout *folderOptionsLayout = new QVBoxLayout();
    folderOptionsLayout->addWidget(checkFollowCurrentDoc);
    folderOptionsLayout->addWidget(checkSubFolders);
    folderOptionsLayout->addWidget(checkHiddenFolders);
    folderOptionsLayout->addStretch();

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addLayout(optionsLayout);
    bottomLayout->addWidget(searchMode);
    bottomLayout->addLayout(folderOptionsLayout);

    QPushButton *buttonFindAll = new QPushButton(tr("Find All"));
    QPushButton *buttonReplaceInFiles = new QPushButton(tr("Replace in Files"));
    QPushButton *buttonClose = new QPushButton(tr("Close"));
    buttonFindAll->setDefault(true);

    connect(buttonFindAll, &QPushButton::clicked, this, &FindInFilesDialog::findAll);
    connect(buttonReplaceInFiles, &QPushButton::clicked, this, &FindInFilesDialog::replaceInFiles);
    connect(buttonClose, &QPushButton::clicked, this, &FindInFilesDialog::close);

    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    buttonsLayout->addWidget(buttonFindAll);
    buttonsLayout->addWidget(buttonReplaceInFiles);
    buttonsLayout->addWidget(buttonClose);
    buttonsLayout->addStretch();

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addLayout(form, 1);
    topLayout->addLayout(buttonsLayout);

    labelStatus = new QLabel();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addWidget(labelStatus);

    // Restore the previous state
    loadHistory(comboFind, QStringLiteral("FindInFiles/FindHistory"));
    loadHistory(comboReplace, QStringLiteral("FindInFiles/ReplaceHistory"));
    loadHistory(comboFilters, QStringLiteral("FindInFiles/FilterHistory"));
    loadHistory(comboDirectory, QStringLiteral("FindInFiles/DirectoryHistory"));

    if (comboFilters->currentText().isEmpty()) {
        comboFilters->setCurrentText(QStringLiteral("*.*"));
    }

    ApplicationSettings *s = settings();
    checkFollowCurrentDoc->setChecked(s->value("FindInFiles/FollowCurrentDoc", true).toBool());
    checkSubFolders->setChecked(s->value("FindInFiles/SubFolders", true).toBool());
    checkHiddenFolders->setChecked(s->value("FindInFiles/HiddenFolders", false).toBool());
    checkWholeWord->setChecked(s->value("FindInFiles/WholeWord", false).toBool());
    checkMatchCase->setChecked(s->value("FindInFiles/MatchCase", false).toBool());
    const int mode = s->value("FindInFiles/SearchMode", 0).toInt();
    (mode == 2 ? radioRegex : mode == 1 ? radioExtended : radioNormal)->setChecked(true);
    checkDotMatchesNewline->setChecked(s->value("FindInFiles/DotMatchesNewline", false).toBool());

    connect(this, &QDialog::finished, this, [=]() {
        ApplicationSettings *s = settings();
        s->setValue("FindInFiles/FollowCurrentDoc", checkFollowCurrentDoc->isChecked());
        s->setValue("FindInFiles/SubFolders", checkSubFolders->isChecked());
        s->setValue("FindInFiles/HiddenFolders", checkHiddenFolders->isChecked());
        s->setValue("FindInFiles/WholeWord", checkWholeWord->isChecked());
        s->setValue("FindInFiles/MatchCase", checkMatchCase->isChecked());
        s->setValue("FindInFiles/SearchMode", radioRegex->isChecked() ? 2 : radioExtended->isChecked() ? 1 : 0);
        s->setValue("FindInFiles/DotMatchesNewline", checkDotMatchesNewline->isChecked());
    });
}

void FindInFilesDialog::setFindText(const QString &text)
{
    if (!text.isEmpty()) {
        comboFind->setCurrentText(text);
    }
    comboFind->lineEdit()->selectAll();
}

void FindInFilesDialog::setDirectory(const QString &directory)
{
    comboDirectory->setCurrentText(QDir::toNativeSeparators(directory));
}

void FindInFilesDialog::setSearchResultsHandler(ISearchResultsHandler *handler)
{
    searchResultsHandler = handler;
}

void FindInFilesDialog::showEvent(QShowEvent *event)
{
    ScintillaNext *editor = window->currentEditor();

    if (checkFollowCurrentDoc->isChecked() && editor && editor->isFile()) {
        setDirectory(editor->getPath());
    }

    comboFind->setFocus();

    QDialog::showEvent(event);
}

QString FindInFilesDialog::convertExtended(const QString &text)
{
    QString result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);

        if (c != QLatin1Char('\\') || i + 1 >= text.size()) {
            result.append(c);
            continue;
        }

        const QChar next = text.at(++i);
        switch (next.unicode()) {
        case 'n': result.append(QLatin1Char('\n')); break;
        case 'r': result.append(QLatin1Char('\r')); break;
        case 't': result.append(QLatin1Char('\t')); break;
        case '0': result.append(QChar(0)); break;
        case '\\': result.append(QLatin1Char('\\')); break;
        case 'x':
        case 'u': {
            const int digits = next == QLatin1Char('x') ? 2 : 4;
            bool ok = false;
            const uint value = text.mid(i + 1, digits).toUInt(&ok, 16);

            if (ok && text.mid(i + 1, digits).size() == digits) {
                result.append(QChar(value));
                i += digits;
            }
            else {
                result.append(QLatin1Char('\\'));
                result.append(next);
            }
            break;
        }
        default:
            result.append(QLatin1Char('\\'));
            result.append(next);
            break;
        }
    }

    return result;
}

bool FindInFilesDialog::buildRegex(QRegularExpression &regex)
{
    const QString text = comboFind->currentText();

    if (text.isEmpty()) {
        labelStatus->setText(tr("<font color=\"red\">Find what is empty.</font>"));
        return false;
    }

    QString pattern;

    if (radioRegex->isChecked()) {
        pattern = text;
    }
    else {
        const QString literal = radioExtended->isChecked() ? convertExtended(text) : text;
        pattern = QRegularExpression::escape(literal);

        if (checkWholeWord->isChecked()) {
            pattern = QStringLiteral("(?<!\\w)(?:%1)(?!\\w)").arg(pattern);
        }
    }

    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::MultilineOption;

    if (!checkMatchCase->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    if (radioRegex->isChecked() && checkDotMatchesNewline->isChecked()) {
        options |= QRegularExpression::DotMatchesEverythingOption;
    }

    regex = QRegularExpression(pattern, options);

    if (!regex.isValid()) {
        labelStatus->setText(tr("<font color=\"red\">Invalid regular expression: %1</font>").arg(regex.errorString().toHtmlEscaped()));
        return false;
    }

    return true;
}

void FindInFilesDialog::browseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder to search from"), comboDirectory->currentText());

    if (!dir.isEmpty()) {
        setDirectory(dir);
    }
}

static bool matchesAnyPattern(const QString &fileName, const QList<QRegularExpression> &patterns)
{
    for (const QRegularExpression &pattern : patterns) {
        if (pattern.match(fileName).hasMatch()) {
            return true;
        }
    }

    return false;
}

QStringList FindInFilesDialog::collectFiles(bool &cancelled)
{
    cancelled = false;

    const QString directory = QDir::fromNativeSeparators(comboDirectory->currentText().trimmed());
    QStringList files;

    if (directory.isEmpty() || !QDir(directory).exists()) {
        labelStatus->setText(tr("<font color=\"red\">The directory does not exist.</font>"));
        return files;
    }

    // Filters are separated by spaces or semicolons. Filters starting with ! exclude files (e.g. "*.cpp *.h !*.moc.cpp")
    QList<QRegularExpression> includes;
    QList<QRegularExpression> excludes;

    const QStringList filters = comboFilters->currentText().split(QRegularExpression(QStringLiteral("[\\s;]+")), Qt::SkipEmptyParts);
    for (QString filter : filters) {
        bool exclude = filter.startsWith(QLatin1Char('!'));

        if (exclude) {
            filter.remove(0, 1);
        }

        // "*.*" in Notepad++ means all files, even ones without an extension
        if (filter == QLatin1String("*.*")) {
            filter = QStringLiteral("*");
        }

        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(filter), QRegularExpression::CaseInsensitiveOption);
        (exclude ? excludes : includes).append(re);
    }

    if (includes.isEmpty()) {
        includes.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*"))));
    }

    QDir::Filters dirFilters = QDir::Files | QDir::NoDotAndDotDot;
    if (checkHiddenFolders->isChecked()) {
        dirFilters |= QDir::Hidden;
    }

    QDirIterator::IteratorFlags flags = checkSubFolders->isChecked() ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(directory, dirFilters, flags);

    int count = 0;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString fileName = it.fileName();

        if (!checkHiddenFolders->isChecked()) {
            // Skip anything inside of a hidden directory
            const QString relative = QDir(directory).relativeFilePath(filePath);
            if (relative.startsWith(QLatin1Char('.')) || relative.contains(QLatin1String("/."))) {
                continue;
            }
        }

        if (matchesAnyPattern(fileName, includes) && !matchesAnyPattern(fileName, excludes)) {
            files.append(filePath);
        }

        if (++count % 500 == 0) {
            QApplication::processEvents();
        }
    }

    files.sort();

    return files;
}

static bool readTextFile(const QString &filePath, QString &text, QByteArray &encoding, QByteArray &bom)
{
    QFile file(filePath);

    if (file.size() > MAX_FILE_SIZE || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();

    // Skip binary files (UTF-16 files have NUL bytes but also have a BOM)
    const bool hasUtf16Bom = data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF");
    if (!hasUtf16Bom && data.left(8000).contains('\0')) {
        return false;
    }

    text = TextEncoding::decode(data, &encoding, &bom);

    return true;
}

void FindInFilesDialog::findAll()
{
    QRegularExpression regex;

    if (!buildRegex(regex) || searchResultsHandler == nullptr) {
        return;
    }

    addToHistory(comboFind, QStringLiteral("FindInFiles/FindHistory"));
    addToHistory(comboFilters, QStringLiteral("FindInFiles/FilterHistory"));
    addToHistory(comboDirectory, QStringLiteral("FindInFiles/DirectoryHistory"));

    bool cancelled = false;
    const QStringList files = collectFiles(cancelled);

    QProgressDialog progress(tr("Searching..."), tr("Cancel"), 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    searchResultsHandler->newSearch(comboFind->currentText());

    int totalHits = 0;
    int filesWithHits = 0;

    for (int i = 0; i < files.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            break;
        }

        const QString &filePath = files.at(i);
        QString text;
        QByteArray encoding;
        QByteArray bom;

        if (!readTextFile(filePath, text, encoding, bom)) {
            continue;
        }

        bool fileEntryAdded = false;

        // Search the entire text so multi-line regular expressions work
        QRegularExpressionMatchIterator matches = regex.globalMatch(text);

        int lineNumber = 0;
        int lineStart = 0;
        int searchedUpTo = 0;

        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();

            if (match.capturedLength() == 0) {
                continue;
            }

            const int start = match.capturedStart();

            // Advance the line counter up to the match
            for (int p = searchedUpTo; p < start; ++p) {
                if (text.at(p) == QLatin1Char('\n')) {
                    lineNumber++;
                    lineStart = p + 1;
                }
            }
            searchedUpTo = start;

            int lineEnd = text.indexOf(QLatin1Char('\n'), start);
            if (lineEnd == -1) {
                lineEnd = text.size();
            }

            QString line = text.mid(lineStart, lineEnd - lineStart);
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }

            if (!fileEntryAdded) {
                searchResultsHandler->newFilePathEntry(QDir::toNativeSeparators(filePath));
                fileEntryAdded = true;
                filesWithHits++;
            }

            // Positions are reported as UTF-8 byte offsets, the same as Scintilla uses
            const int matchEndInLine = qMin(match.capturedEnd(), lineStart + static_cast<int>(line.size()));
            const int startByte = text.mid(lineStart, start - lineStart).toUtf8().size();
            const int endByte = text.mid(lineStart, matchEndInLine - lineStart).toUtf8().size();

            searchResultsHandler->newResultsEntry(line, lineNumber, startByte, endByte);
            totalHits++;
        }
    }

    progress.setValue(files.size());

    searchResultsHandler->completeSearch();

    labelStatus->setText(tr("<font color=\"green\">%Ln hit(s) in %1 file(s) (%2 file(s) searched)</font>", "", totalHits).arg(filesWithHits).arg(files.size()));
}

QString FindInFilesDialog::expandReplacement(const QRegularExpressionMatch &match, const QString &replacement) const
{
    // Supports $0-$9 and \0-\9 back references like Notepad++
    QString result;

    for (int i = 0; i < replacement.size(); ++i) {
        const QChar c = replacement.at(i);

        if ((c == QLatin1Char('$') || c == QLatin1Char('\\')) && i + 1 < replacement.size() && replacement.at(i + 1).isDigit()) {
            result.append(match.captured(replacement.at(i + 1).digitValue()));
            i++;
        }
        else if (c == QLatin1Char('\\') && i + 1 < replacement.size()) {
            const QChar next = replacement.at(++i);
            switch (next.unicode()) {
            case 'n': result.append(QLatin1Char('\n')); break;
            case 'r': result.append(QLatin1Char('\r')); break;
            case 't': result.append(QLatin1Char('\t')); break;
            default: result.append(next); break;
            }
        }
        else {
            result.append(c);
        }
    }

    return result;
}

void FindInFilesDialog::replaceInFiles()
{
    QRegularExpression regex;

    if (!buildRegex(regex)) {
        return;
    }

    const QString directory = comboDirectory->currentText();
    const QString message = tr("Are you sure you want to replace all occurrences in all files matching <b>%1</b> in:<br><b>%2</b>?")
                                .arg(comboFilters->currentText().toHtmlEscaped(), directory.toHtmlEscaped());

    if (QMessageBox::question(this, tr("Replace in Files"), message) != QMessageBox::Yes) {
        return;
    }

    addToHistory(comboFind, QStringLiteral("FindInFiles/FindHistory"));
    addToHistory(comboReplace, QStringLiteral("FindInFiles/ReplaceHistory"));
    addToHistory(comboFilters, QStringLiteral("FindInFiles/FilterHistory"));
    addToHistory(comboDirectory, QStringLiteral("FindInFiles/DirectoryHistory"));

    QString replacement = comboReplace->currentText();
    if (radioExtended->isChecked()) {
        replacement = convertExtended(replacement);
    }

    bool cancelled = false;
    const QStringList files = collectFiles(cancelled);

    QProgressDialog progress(tr("Replacing..."), tr("Cancel"), 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    int totalReplacements = 0;
    int filesChanged = 0;
    QStringList failures;

    for (int i = 0; i < files.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            break;
        }

        const QString &filePath = files.at(i);
        QString text;
        QByteArray encoding;
        QByteArray bom;

        if (!readTextFile(filePath, text, encoding, bom)) {
            continue;
        }

        QString result;
        int last = 0;
        int count = 0;

        QRegularExpressionMatchIterator matches = regex.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();

            if (match.capturedLength() == 0) {
                continue;
            }

            result.append(QStringView(text).mid(last, match.capturedStart() - last));
            result.append(radioRegex->isChecked() ? expandReplacement(match, replacement) : replacement);
            last = match.capturedEnd();
            count++;
        }

        if (count == 0) {
            continue;
        }

        result.append(QStringView(text).mid(last));

        QSaveFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bom);
            file.write(TextEncoding::encode(result, encoding));

            if (file.commit()) {
                totalReplacements += count;
                filesChanged++;
                continue;
            }
        }

        failures.append(QDir::toNativeSeparators(filePath));
    }

    progress.setValue(files.size());

    labelStatus->setText(tr("<font color=\"green\">Replace in Files: %Ln occurrence(s) were replaced in %1 file(s).</font>", "", totalReplacements).arg(filesChanged));

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, tr("Replace in Files"), tr("The following files could not be written:\n%1").arg(failures.join(QLatin1Char('\n'))));
    }

    // Let the main window know so any open files get reloaded
    window->focusIn();
}

void FindInFilesDialog::addToHistory(QComboBox *combo, const QString &settingsKey)
{
    const QString text = combo->currentText();

    if (text.isEmpty()) {
        return;
    }

    const QSignalBlocker blocker(combo);
    combo->removeItem(combo->findText(text));
    combo->insertItem(0, text);
    combo->setCurrentIndex(0);

    while (combo->count() > 15) {
        combo->removeItem(combo->count() - 1);
    }

    QStringList items;
    for (int i = 0; i < combo->count(); ++i) {
        items.append(combo->itemText(i));
    }

    settings()->setValue(settingsKey, items);
}

void FindInFilesDialog::loadHistory(QComboBox *combo, const QString &settingsKey)
{
    combo->addItems(settings()->value(settingsKey).toStringList());
}
