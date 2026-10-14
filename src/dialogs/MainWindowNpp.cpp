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

// This file contains the parts of the MainWindow that add the menus, commands, panels and
// dialogs that make Notepad Next behave like Notepad++.

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "ApplicationSettings.h"
#include "BookMarkDecorator.h"
#include "DefaultDirectoryManager.h"
#include "DockedEditor.h"
#include "EditOperations.h"
#include "EditorManager.h"
#include "FileMonitor.h"
#include "HideLinesDecorator.h"
#include "LuaState.h"
#include "MarkerAppDecorator.h"
#include "NotepadNextApplication.h"
#include "ScintillaSorter.h"
#include "ScrollSynchronizer.h"
#include "TextEncoding.h"
#include "UndoAction.h"

#include "AutoCompletion.h"

#include "CharacterPanelDock.h"
#include "ClipboardHistoryDock.h"
#include "DocumentMapDock.h"
#include "FunctionListDock.h"
#include "SearchResultsDock.h"

#include "FindCharactersInRangeDialog.h"
#include "FindInFilesDialog.h"
#include "FindReplaceDialog.h"
#include "HashGeneratorDialog.h"
#include "MarkDialog.h"
#include "RunDialog.h"
#include "ShortcutMapperDialog.h"
#include "WindowsDialog.h"

#include "EditorPrintPreviewRenderer.h"
#include "FadingIndicator.h"

#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QCryptographicHash>
#include <QDockWidget>
#include <QFontDatabase>
#include <QFileDialog>
#include <QInputDialog>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPrinter>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "DockAreaWidget.h"

#include <algorithm>
#include <functional>


namespace {

// Inserts the actions into the menu right after an existing action
void insertActionsAfter(QMenu *menu, QAction *after, const QList<QAction *> &actions)
{
    const QList<QAction *> existing = menu->actions();
    const int index = existing.indexOf(after);
    QAction *before = (index >= 0 && index + 1 < existing.size()) ? existing.at(index + 1) : Q_NULLPTR;

    for (QAction *action : actions) {
        if (before) {
            menu->insertAction(before, action);
        }
        else {
            menu->addAction(action);
        }
    }
}

QAction *createSeparator(QObject *parent)
{
    QAction *separator = new QAction(parent);
    separator->setSeparator(true);
    return separator;
}

}

struct EncodingChoice {
    QByteArray encoding;
    int bom;
};

Q_DECLARE_METATYPE(EncodingChoice)


QAction *MainWindow::createAction(const QString &name, const QString &text, const QKeySequence &shortcut)
{
    QAction *a = new QAction(text, this);
    a->setObjectName(QStringLiteral("action") + name);

    if (!shortcut.isEmpty()) {
        a->setShortcut(shortcut);
    }

    // Make sure the shortcut works even if the action is only in a menu that is hidden
    addAction(a);

    return a;
}

QAction *MainWindow::action(const QString &name) const
{
    return findChild<QAction *>(QStringLiteral("action") + name, Qt::FindDirectChildrenOnly);
}

void MainWindow::setupNotepadPlusPlusFeatures()
{
    fileMonitor = new FileMonitor(this);
    scrollSynchronizer = new ScrollSynchronizer(this);

    setupNppDocks();
    setupNppFileMenu();
    setupNppEditMenu();
    setupNppSearchMenu();
    setupNppViewMenu();
    setupNppEncodingMenu();
    setupNppSettingsMenu();
    setupNppToolsMenu();
    setupNppMacroMenu();
    setupNppRunMenu();
    setupNppWindowMenu();
    setupNppHelpMenu();

    // Actions that only make sense when text is selected
    for (const QString &name : {QStringLiteral("ProperCase"), QStringLiteral("ProperCaseBlend"), QStringLiteral("SentenceCase"),
                                QStringLiteral("SentenceCaseBlend"), QStringLiteral("InvertCase"), QStringLiteral("RandomCase"),
                                QStringLiteral("SearchOnInternet"), QStringLiteral("OpenFileInSelection")}) {
        selectionBasedActions.append(action(name));
    }

    // Apply the custom shortcuts now that all the actions exist
    applyCustomShortcuts();

    connect(this, &MainWindow::editorActivated, this, [=](ScintillaNext *editor) {
        if (scrollSynchronizer->isEnabled()) {
            scrollSynchronizer->setEditors(editor, dockedEditor->editorInOtherView(editor));
        }

        // Begin/End Select only works within a single editor
        if (beginSelectEditor && beginSelectEditor != editor) {
            beginSelectEditor.clear();
            beginSelectPosition = -1;
            action(QStringLiteral("BeginEndSelect"))->setChecked(false);
        }
    });

    connect(fileMonitor, &FileMonitor::monitoringChanged, this, [=](ScintillaNext *editor, bool) {
        dockedEditor->refreshEditorIcon(editor);
        if (editor == currentEditor()) {
            updateNppUi(editor);
        }
    });
}

void MainWindow::setupNppDocks()
{
    DocumentMapDock *documentMap = new DocumentMapDock(this);
    documentMap->hide();
    addDockWidget(Qt::RightDockWidgetArea, documentMap);

    FunctionListDock *functionList = new FunctionListDock(this);
    functionList->hide();
    addDockWidget(Qt::RightDockWidgetArea, functionList);

    ClipboardHistoryDock *clipboardHistory = new ClipboardHistoryDock(this);
    clipboardHistory->hide();
    addDockWidget(Qt::LeftDockWidgetArea, clipboardHistory);

    CharacterPanelDock *characterPanel = new CharacterPanelDock(this);
    characterPanel->hide();
    addDockWidget(Qt::LeftDockWidgetArea, characterPanel);

    // The actions need to be owned by the main window so they can be found by name (shortcuts, toolbar, etc)
    const QList<QPair<QDockWidget *, QString>> docks = {
        {documentMap, QStringLiteral("actionDocumentMap")},
        {functionList, QStringLiteral("actionFunctionList")},
        {clipboardHistory, QStringLiteral("actionClipboardHistory")},
        {characterPanel, QStringLiteral("actionCharacterPanel")},
    };

    for (const auto &dock : docks) {
        QAction *toggle = dock.first->toggleViewAction();
        toggle->setParent(this);
        toggle->setObjectName(dock.second);
        addAction(toggle);
    }

    // Search results from files that are not opened yet (Find in Files)
    SearchResultsDock *srDock = findChild<SearchResultsDock *>();
    connect(srDock, &SearchResultsDock::searchResultInFileActivated, this, &MainWindow::goToSearchResultInFile);
}

void MainWindow::goToSearchResultInFile(const QString &filePath, int lineNumber, int startPositionFromBeginning, int endPositionFromBeginning)
{
    openFile(QDir::fromNativeSeparators(filePath));

    ScintillaNext *editor = app->getEditorManager()->getEditorByFilePath(QDir::fromNativeSeparators(filePath));
    if (editor == Q_NULLPTR) {
        return;
    }

    dockedEditor->switchToEditor(editor);

    const int linePos = editor->positionFromLine(lineNumber);
    editor->goToRange({linePos + startPositionFromBeginning, linePos + endPositionFromBeginning});
    editor->verticalCentreCaret();
    editor->grabFocus();
}

// ---------------------------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------------------------

void MainWindow::setupNppFileMenu()
{
    // "Open Containing Folder" works on all platforms, not just Windows
#if defined(Q_OS_MACOS)
    ui->actionShowInExplorer->setText(tr("Show in Finder"));
    ui->actionOpenTerminalHere->setText(tr("Open Terminal Here"));
#elif !defined(Q_OS_WIN)
    ui->actionShowInExplorer->setText(tr("Show in File Manager"));
    ui->actionOpenTerminalHere->setText(tr("Open Terminal Here"));
#endif

#ifndef Q_OS_WIN
    connect(ui->actionShowInExplorer, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        if (!editor->isFile()) return;

#ifdef Q_OS_MACOS
        QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), editor->getFilePath()});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(editor->getPath()));
#endif
    });

    connect(ui->actionOpenTerminalHere, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        const QString dir = editor->isFile() ? editor->getPath() : QDir::homePath();

#ifdef Q_OS_MACOS
        QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-a"), QStringLiteral("Terminal"), dir});
#else
        // Try the common terminal emulators
        const QString custom = app->getSettings()->value("App/TerminalCommand").toString();
        const QStringList terminals = custom.isEmpty() ? QStringList{QStringLiteral("x-terminal-emulator"), QStringLiteral("gnome-terminal"), QStringLiteral("konsole"), QStringLiteral("xfce4-terminal"), QStringLiteral("xterm")} : QStringList{custom};

        for (const QString &terminal : terminals) {
            if (!QStandardPaths::findExecutable(terminal).isEmpty() && QProcess::startDetached(terminal, {}, dir)) {
                break;
            }
        }
#endif
    });
#endif

    QMenu *menuOpenContainingFolder = new QMenu(tr("Open Containing Folder"), this);
    menuOpenContainingFolder->addAction(ui->actionShowInExplorer);
    menuOpenContainingFolder->addAction(ui->actionOpenTerminalHere);

    QAction *openInDefaultViewer = createAction(QStringLiteral("OpenInDefaultViewer"), tr("Open in Default Viewer"));
    connect(openInDefaultViewer, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        if (editor->isFile()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(editor->getFilePath()));
        }
    });

    insertActionsAfter(ui->menuFile, ui->actionOpen, {menuOpenContainingFolder->menuAction(), openInDefaultViewer});

    // Close All Unchanged
    QAction *closeAllUnchanged = createAction(QStringLiteral("CloseAllUnchanged"), tr("Close All Unchanged"));
    connect(closeAllUnchanged, &QAction::triggered, this, [=]() {
        for (ScintillaNext *editor : editors()) {
            if (editor->isSavedToDisk()) {
                editor->close();
            }
        }

        if (editorCount() == 0) {
            newFile();
        }
    });
    ui->menuClose_More->addAction(closeAllUnchanged);

    // Sessions
    QAction *loadSession = createAction(QStringLiteral("LoadSession"), tr("Load Session..."));
    QAction *saveSession = createAction(QStringLiteral("SaveSession"), tr("Save Session..."));

    connect(loadSession, &QAction::triggered, this, [=]() {
        const QString fileName = QFileDialog::getOpenFileName(this, tr("Load Session"), defaultDirectoryManager->getDefaultDirectory(), tr("Session files (*.xml *.session);;All files (*)"));
        if (!fileName.isEmpty() && !loadNppSession(fileName)) {
            QMessageBox::warning(this, tr("Load Session"), tr("<b>%1</b> is not a valid session file.").arg(fileName.toHtmlEscaped()));
        }
    });

    connect(saveSession, &QAction::triggered, this, [=]() {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save Session"), defaultDirectoryManager->getDefaultDirectory(), tr("Session files (*.xml *.session);;All files (*)"));
        if (fileName.isEmpty()) return;

        if (QFileInfo(fileName).suffix().isEmpty()) {
            fileName += QStringLiteral(".xml");
        }

        if (!saveNppSession(fileName)) {
            QMessageBox::warning(this, tr("Save Session"), tr("Unable to save the session to <b>%1</b>.").arg(fileName.toHtmlEscaped()));
        }
    });

    ui->menuFile->insertActions(ui->actionPrint, {loadSession, saveSession, createSeparator(this)});

    // Print Now (prints without the preview dialog)
    QAction *printNow = createAction(QStringLiteral("PrintNow"), tr("Print Now"));
    connect(printNow, &QAction::triggered, this, [=]() {
        QPrinter printer;
        printer.setPageMargins(QMarginsF(.5, .5, .5, .5), QPageLayout::Inch);
        EditorPrintPreviewRenderer renderer(currentEditor());
        renderer.render(&printer);
    });
    insertActionsAfter(ui->menuFile, ui->actionPrint, {printNow});

    ui->actionPrint->setText(tr("Print..."));
}

bool MainWindow::saveNppSession(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    ScintillaNext *current = currentEditor();
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("NotepadPlus"));
    xml.writeStartElement(QStringLiteral("Session"));
    xml.writeAttribute(QStringLiteral("activeView"), QStringLiteral("0"));

    // The first view is the "mainView", any other view is the "subView"
    QVector<ScintillaNext *> mainView;
    QVector<ScintillaNext *> subView;

    for (ScintillaNext *editor : editors()) {
        if (mainView.isEmpty() || dockedEditor->editorsInSameView(mainView.first()).contains(editor)) {
            mainView.append(editor);
        }
        else {
            subView.append(editor);
        }
    }

    auto writeView = [&](const QString &name, const QVector<ScintillaNext *> &view) {
        int activeIndex = 0;
        QVector<ScintillaNext *> files;

        for (ScintillaNext *editor : view) {
            if (editor->isFile()) {
                if (editor == current) {
                    activeIndex = files.size();
                }
                files.append(editor);
            }
        }

        xml.writeStartElement(name);
        xml.writeAttribute(QStringLiteral("activeIndex"), QString::number(activeIndex));

        for (ScintillaNext *editor : files) {
            xml.writeStartElement(QStringLiteral("File"));
            xml.writeAttribute(QStringLiteral("firstVisibleLine"), QString::number(editor->docLineFromVisible(editor->firstVisibleLine())));
            xml.writeAttribute(QStringLiteral("xOffset"), QString::number(editor->xOffset()));
            xml.writeAttribute(QStringLiteral("startPos"), QString::number(editor->selectionNAnchor(editor->mainSelection())));
            xml.writeAttribute(QStringLiteral("endPos"), QString::number(editor->selectionNCaret(editor->mainSelection())));
            xml.writeAttribute(QStringLiteral("lang"), editor->languageName);
            // Notepad++ stores a number here, -1 lets it auto detect. Our encoding name is stored separately.
            xml.writeAttribute(QStringLiteral("encoding"), QStringLiteral("-1"));
            xml.writeAttribute(QStringLiteral("nnEncoding"), QString::fromLatin1(editor->encoding()));
            xml.writeAttribute(QStringLiteral("userReadOnly"), editor->readOnly() ? QStringLiteral("yes") : QStringLiteral("no"));
            xml.writeAttribute(QStringLiteral("filename"), QDir::toNativeSeparators(editor->getFilePath()));
            xml.writeEndElement();
        }

        xml.writeEndElement();
    };

    writeView(QStringLiteral("mainView"), mainView);
    writeView(QStringLiteral("subView"), subView);

    xml.writeEndElement(); // Session
    xml.writeEndElement(); // NotepadPlus
    xml.writeEndDocument();

    return !xml.hasError();
}

bool MainWindow::loadNppSession(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    struct SessionFile {
        QString fileName;
        int firstVisibleLine;
        int startPos;
        int endPos;
        QString language;
        QByteArray encoding;
        bool readOnly;
    };

    QList<SessionFile> files;
    bool foundSession = false;
    QXmlStreamReader xml(&file);

    while (!xml.atEnd()) {
        xml.readNext();

        if (!xml.isStartElement()) {
            continue;
        }

        if (xml.name() == QLatin1String("Session")) {
            foundSession = true;
        }
        else if (xml.name() == QLatin1String("File")) {
            const QXmlStreamAttributes attributes = xml.attributes();
            SessionFile f;
            f.fileName = QDir::fromNativeSeparators(attributes.value(QLatin1String("filename")).toString());
            f.firstVisibleLine = attributes.value(QLatin1String("firstVisibleLine")).toInt();
            f.startPos = attributes.value(QLatin1String("startPos")).toInt();
            f.endPos = attributes.value(QLatin1String("endPos")).toInt();
            f.language = attributes.value(QLatin1String("lang")).toString();
            f.encoding = attributes.value(QLatin1String("nnEncoding")).toString().toLatin1();
            f.readOnly = attributes.value(QLatin1String("userReadOnly")).toString() == QLatin1String("yes");

            if (!f.fileName.isEmpty()) {
                files.append(f);
            }
        }
    }

    if (xml.hasError() || !foundSession) {
        return false;
    }

    QStringList missing;

    for (const SessionFile &f : files) {
        if (!QFileInfo::exists(f.fileName)) {
            missing.append(QDir::toNativeSeparators(f.fileName));
            continue;
        }

        openFile(f.fileName);

        ScintillaNext *editor = app->getEditorManager()->getEditorByFilePath(f.fileName);
        if (editor == Q_NULLPTR) {
            continue;
        }

        // Notepad++ stores its own encoding numbers, only use names that are actual encodings
        if (!f.encoding.isEmpty() && TextEncoding::isAvailable(f.encoding) && TextEncoding::canonicalName(f.encoding) != TextEncoding::canonicalName(editor->encoding())) {
            if (editor->isSavedToDisk()) {
                editor->reloadWithEncoding(f.encoding);
            }
        }

        if (!f.language.isEmpty() && app->getLanguages().contains(f.language)) {
            setLanguage(editor, f.language);
        }

        const int length = editor->length();
        editor->setSelection(qBound(0, f.endPos, length), qBound(0, f.startPos, length));
        editor->setFirstVisibleLine(editor->visibleFromDocLine(f.firstVisibleLine));

        if (f.readOnly) {
            editor->setReadOnly(true);
            dockedEditor->refreshEditorIcon(editor);
        }
    }

    if (!missing.isEmpty()) {
        QMessageBox::information(this, tr("Load Session"), tr("The following files no longer exist:\n%1").arg(missing.join(QLatin1Char('\n'))));
    }

    return true;
}

// ---------------------------------------------------------------------------------------------
// Edit
// ---------------------------------------------------------------------------------------------

void MainWindow::setupNppEditMenu()
{
    // Multi-select All / Begin-End select
    QAction *selectAllOccurrences = createAction(QStringLiteral("SelectAllOccurrences"), tr("Multi-select All Occurrences"));
    connect(selectAllOccurrences, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        editor->setSearchFlags(editor->selectionEmpty() ? (SCFIND_MATCHCASE | SCFIND_WHOLEWORD) : SCFIND_MATCHCASE);
        editor->targetWholeDocument();
        editor->multipleSelectAddEach();
    });

    QAction *beginEndSelect = createAction(QStringLiteral("BeginEndSelect"), tr("Begin/End Select"));
    beginEndSelect->setCheckable(true);
    connect(beginEndSelect, &QAction::triggered, this, [=](bool checked) {
        ScintillaNext *editor = currentEditor();

        if (checked) {
            beginSelectEditor = editor;
            beginSelectPosition = editor->currentPos();
        }
        else if (beginSelectEditor == editor && beginSelectPosition >= 0) {
            editor->setSelection(editor->currentPos(), qMin(beginSelectPosition, static_cast<int>(editor->length())));
            beginSelectEditor.clear();
            beginSelectPosition = -1;
        }
    });

    insertActionsAfter(ui->menuEdit, ui->actionSelectNext, {selectAllOccurrences, beginEndSelect});

    // Insert > Date Time
    QMenu *menuInsert = new QMenu(tr("Insert"), this);
    QAction *dateTimeShort = createAction(QStringLiteral("InsertDateTimeShort"), tr("Date Time (short)"));
    QAction *dateTimeLong = createAction(QStringLiteral("InsertDateTimeLong"), tr("Date Time (long)"));
    QAction *dateTimeCustom = createAction(QStringLiteral("InsertDateTimeCustom"), tr("Date Time (customized)"));
    QAction *dateTimeFormat = createAction(QStringLiteral("CustomizeDateTimeFormat"), tr("Customize Date Time Format..."));
    connect(dateTimeShort, &QAction::triggered, this, [=]() { insertDateTime(0); });
    connect(dateTimeLong, &QAction::triggered, this, [=]() { insertDateTime(1); });
    connect(dateTimeCustom, &QAction::triggered, this, [=]() { insertDateTime(2); });
    connect(dateTimeFormat, &QAction::triggered, this, [=]() {
        ApplicationSettings *settings = app->getSettings();
        bool ok;
        const QString format = QInputDialog::getText(this, tr("Customize Date Time Format"),
                                                     tr("Format (e.g. yyyy-MM-dd HH:mm:ss, dddd d MMMM yyyy):"), QLineEdit::Normal,
                                                     settings->value("Editor/DateTimeFormat", QStringLiteral("yyyy-MM-dd HH:mm:ss")).toString(), &ok);
        if (ok && !format.isEmpty()) {
            settings->setValue("Editor/DateTimeFormat", format);
        }
    });
    menuInsert->addActions({dateTimeShort, dateTimeLong, dateTimeCustom});
    menuInsert->addSeparator();
    menuInsert->addAction(dateTimeFormat);
    ui->menuEdit->insertMenu(ui->menuCopyMore->menuAction(), menuInsert);

    // Copy to clipboard
    QAction *copyAllNames = createAction(QStringLiteral("CopyAllFileNames"), tr("Copy All File Names"));
    QAction *copyAllPaths = createAction(QStringLiteral("CopyAllFilePaths"), tr("Copy All File Paths"));
    connect(copyAllNames, &QAction::triggered, this, [=]() {
        QStringList names;
        for (ScintillaNext *editor : editors()) names.append(editor->getName());
        QApplication::clipboard()->setText(names.join(QLatin1Char('\n')));
    });
    connect(copyAllPaths, &QAction::triggered, this, [=]() {
        QStringList paths;
        for (ScintillaNext *editor : editors()) paths.append(editor->isFile() ? QDir::toNativeSeparators(editor->getFilePath()) : editor->getName());
        QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
    });
    ui->menuCopyMore->addSeparator();
    ui->menuCopyMore->addActions({copyAllNames, copyAllPaths});

    // Convert Case
    ui->actionUpperCase->setText(tr("&UPPERCASE"));
    ui->actionLowerCase->setText(tr("&lowercase"));
    ui->actionUpperCase->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+U")));
    ui->actionLowerCase->setShortcut(QKeySequence(QStringLiteral("Ctrl+U")));

    struct CaseConversion {
        QString name;
        QString text;
        QKeySequence shortcut;
        std::function<QString(const QString &)> func;
    };

    const QList<CaseConversion> conversions = {
        {QStringLiteral("ProperCase"), tr("&Proper Case"), QKeySequence(QStringLiteral("Alt+U")), [](const QString &s) { return EditOperations::toProperCase(s, false); }},
        {QStringLiteral("ProperCaseBlend"), tr("Proper Case (blend)"), QKeySequence(QStringLiteral("Alt+Shift+U")), [](const QString &s) { return EditOperations::toProperCase(s, true); }},
        {QStringLiteral("SentenceCase"), tr("&Sentence case"), QKeySequence(QStringLiteral("Ctrl+Alt+U")), [](const QString &s) { return EditOperations::toSentenceCase(s, false); }},
        {QStringLiteral("SentenceCaseBlend"), tr("Sentence case (blend)"), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+U")), [](const QString &s) { return EditOperations::toSentenceCase(s, true); }},
        {QStringLiteral("InvertCase"), tr("&iNVERT cASE"), QKeySequence(), EditOperations::toInvertedCase},
        {QStringLiteral("RandomCase"), tr("&ranDOm CasE"), QKeySequence(), EditOperations::toRandomCase},
    };

    for (const CaseConversion &conversion : conversions) {
        QAction *a = createAction(conversion.name, conversion.text, conversion.shortcut);
        auto func = conversion.func;
        connect(a, &QAction::triggered, this, [=]() { EditOperations::transformSelectedText(currentEditor(), func); });
        ui->menuConvertCase->addAction(a);
    }

    // Line operations
    QAction *removeBlankLines = createAction(QStringLiteral("RemoveBlankLines"), tr("Remove Empty Lines (Containing Blank characters)"));
    connect(removeBlankLines, &QAction::triggered, this, [=]() { EditOperations::removeBlankLines(currentEditor()); });
    insertActionsAfter(ui->menuLine_Operations, ui->actionRemoveEmptyLines, {removeBlankLines});

    QAction *insertAbove = createAction(QStringLiteral("InsertBlankLineAbove"), tr("Insert Blank Line Above Current"), QKeySequence(QStringLiteral("Ctrl+Alt+Return")));
    QAction *insertBelow = createAction(QStringLiteral("InsertBlankLineBelow"), tr("Insert Blank Line Below Current"), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+Return")));
    connect(insertAbove, &QAction::triggered, this, [=]() { EditOperations::insertBlankLineAbove(currentEditor()); });
    connect(insertBelow, &QAction::triggered, this, [=]() { EditOperations::insertBlankLineBelow(currentEditor()); });
    insertActionsAfter(ui->menuLine_Operations, ui->actionMoveSelectedLinesDown, {insertAbove, insertBelow});

    QAction *randomize = createAction(QStringLiteral("RandomizeLineOrder"), tr("Randomize Line Order"));
    connect(randomize, &QAction::triggered, this, [=]() {
        ScintillaSorter sorter(currentEditor());
        sorter.sort(RandomSorter());
    });
    insertActionsAfter(ui->menuLine_Operations, ui->actionReverseLineOrder, {randomize});

    auto addSorter = [=](const QString &name, const QString &text, QAction *after, std::function<void(ScintillaSorter &)> func) {
        QAction *a = createAction(name, text);
        connect(a, &QAction::triggered, this, [=]() {
            ScintillaSorter sorter(currentEditor());
            func(sorter);
        });
        insertActionsAfter(ui->menuLine_Operations, after, {a});
        return a;
    };

    QAction *a = addSorter(QStringLiteral("SortLinesLocaleAsc"), tr("Sort Lines In Locale Order Ascending"), ui->actionSortLinesAscCaseInsensitive,
                           [](ScintillaSorter &s) { s.sort(LocaleSorter(Sorter::Direction::Ascending)); });
    a = addSorter(QStringLiteral("SortLinesIntegerAsc"), tr("Sort Lines As Integers Ascending"), a,
                  [](ScintillaSorter &s) { s.sort(IntegerSorter(Sorter::Direction::Ascending)); });
    a = addSorter(QStringLiteral("SortLinesDecimalCommaAsc"), tr("Sort Lines As Decimals (Comma) Ascending"), a,
                  [](ScintillaSorter &s) { s.sort(DecimalSorter(Sorter::Direction::Ascending, true)); });
    addSorter(QStringLiteral("SortLinesDecimalDotAsc"), tr("Sort Lines As Decimals (Dot) Ascending"), a,
              [](ScintillaSorter &s) { s.sort(DecimalSorter(Sorter::Direction::Ascending, false)); });

    a = addSorter(QStringLiteral("SortLinesLocaleDesc"), tr("Sort Lines In Locale Order Descending"), ui->actionSortLinesDescCaseInsensitive,
                  [](ScintillaSorter &s) { s.sort(LocaleSorter(Sorter::Direction::Descending)); });
    a = addSorter(QStringLiteral("SortLinesIntegerDesc"), tr("Sort Lines As Integers Descending"), a,
                  [](ScintillaSorter &s) { s.sort(IntegerSorter(Sorter::Direction::Descending)); });
    a = addSorter(QStringLiteral("SortLinesDecimalCommaDesc"), tr("Sort Lines As Decimals (Comma) Descending"), a,
                  [](ScintillaSorter &s) { s.sort(DecimalSorter(Sorter::Direction::Descending, true)); });
    addSorter(QStringLiteral("SortLinesDecimalDotDesc"), tr("Sort Lines As Decimals (Dot) Descending"), a,
              [](ScintillaSorter &s) { s.sort(DecimalSorter(Sorter::Direction::Descending, false)); });

    // Comment/Uncomment
    QAction *blockComment = createAction(QStringLiteral("BlockComment"), tr("Block Comment"), QKeySequence(QStringLiteral("Ctrl+Shift+Q")));
    QAction *blockUncomment = createAction(QStringLiteral("BlockUncomment"), tr("Block Uncomment"));
    connect(blockComment, &QAction::triggered, this, [=]() {
        if (!EditOperations::blockComment(currentEditor())) {
            FadingIndicator::showText(currentEditor(), tr("Block comments are not supported for %1").arg(currentEditor()->languageName));
        }
    });
    connect(blockUncomment, &QAction::triggered, this, [=]() { EditOperations::blockUncomment(currentEditor()); });
    ui->menuCommentUncomment->addSeparator();
    ui->menuCommentUncomment->addActions({blockComment, blockUncomment});

    // Auto-Completion
    QMenu *menuAutoCompletion = new QMenu(tr("Auto-Completion"), this);
#ifdef Q_OS_MACOS
    const QKeySequence functionCompletionKey(QStringLiteral("Meta+Space"));
    const QKeySequence pathCompletionKey(QStringLiteral("Meta+Alt+Space"));
#else
    const QKeySequence functionCompletionKey(QStringLiteral("Ctrl+Space"));
    const QKeySequence pathCompletionKey(QStringLiteral("Ctrl+Alt+Space"));
#endif
    QAction *functionCompletion = createAction(QStringLiteral("FunctionCompletion"), tr("Function Completion"), functionCompletionKey);
    QAction *wordCompletion = createAction(QStringLiteral("WordCompletion"), tr("Word Completion"), QKeySequence(QStringLiteral("Ctrl+Return")));
    QAction *pathCompletion = createAction(QStringLiteral("PathCompletion"), tr("Path Completion"), pathCompletionKey);

    connect(wordCompletion, &QAction::triggered, this, [=]() {
        AutoCompletion *ac = currentEditor()->findChild<AutoCompletion *>(QString(), Qt::FindDirectChildrenOnly);
        if (ac) ac->showWordCompletion(1);
    });
    connect(functionCompletion, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        AutoCompletion *ac = editor->findChild<AutoCompletion *>(QString(), Qt::FindDirectChildrenOnly);
        if (!ac) return;

        // Use the keywords of the current language like Notepad++'s API files
        app->getLuaState()->setVariable("languageName", editor->languageName);
        const QString keywords = app->getLuaState()->executeAndReturn<QString>(R"(
            local L = languages[languageName]
            local words = {}
            if L and L.keywords then
                for _, list in pairs(L.keywords) do
                    words[#words + 1] = list
                end
            end
            return table.concat(words, " ")
        )");

        ac->showWordCompletion(1, keywords.split(QLatin1Char(' '), Qt::SkipEmptyParts));
    });
    connect(pathCompletion, &QAction::triggered, this, [=]() {
        AutoCompletion *ac = currentEditor()->findChild<AutoCompletion *>(QString(), Qt::FindDirectChildrenOnly);
        if (ac) ac->showPathCompletion();
    });
    menuAutoCompletion->addActions({functionCompletion, wordCompletion, pathCompletion});
    insertActionsAfter(ui->menuEdit, ui->menuCommentUncomment->menuAction(), {menuAutoCompletion->menuAction()});

    // Blank Operations
    QMenu *menuBlank = new QMenu(tr("Blank Operations"), this);
    struct BlankOperation {
        QString name;
        QString text;
        std::function<void(ScintillaNext *)> func;
    };
    const QList<BlankOperation> blankOperations = {
        {QStringLiteral("TrimTrailingSpace"), tr("Trim Trailing Space"), EditOperations::trimTrailingSpace},
        {QStringLiteral("TrimLeadingSpace"), tr("Trim Leading Space"), EditOperations::trimLeadingSpace},
        {QStringLiteral("TrimLeadingAndTrailingSpace"), tr("Trim Leading and Trailing Space"), EditOperations::trimLeadingAndTrailingSpace},
        {QStringLiteral("EolToSpace"), tr("EOL to Space"), [](ScintillaNext *e) { EditOperations::eolToSpace(e, false); }},
        {QStringLiteral("RemoveUnnecessaryBlankAndEol"), tr("Remove Unnecessary Blank and EOL"), [](ScintillaNext *e) { EditOperations::eolToSpace(e, true); }},
        {QString(), QString(), nullptr},
        {QStringLiteral("TabToSpace"), tr("TAB to Space"), EditOperations::tabsToSpaces},
        {QStringLiteral("SpaceToTabAll"), tr("Space to TAB (All)"), [](ScintillaNext *e) { EditOperations::spacesToTabs(e, false); }},
        {QStringLiteral("SpaceToTabLeading"), tr("Space to TAB (Leading)"), [](ScintillaNext *e) { EditOperations::spacesToTabs(e, true); }},
    };
    for (const BlankOperation &operation : blankOperations) {
        if (operation.name.isEmpty()) {
            menuBlank->addSeparator();
            continue;
        }

        QAction *a = createAction(operation.name, operation.text);
        auto func = operation.func;
        connect(a, &QAction::triggered, this, [=]() { func(currentEditor()); });
        menuBlank->addAction(a);
    }
    insertActionsAfter(ui->menuEdit, ui->menuEOLConversion->menuAction(), {menuBlank->menuAction()});

    // On Selection
    QMenu *menuOnSelection = new QMenu(tr("On Selection"), this);
    QAction *openFileInSelection = createAction(QStringLiteral("OpenFileInSelection"), tr("Open File"));
    QAction *searchOnInternet = createAction(QStringLiteral("SearchOnInternet"), tr("Search on Internet"));
    QAction *changeSearchEngine = createAction(QStringLiteral("ChangeSearchEngine"), tr("Change Search Engine..."));
    connect(openFileInSelection, &QAction::triggered, this, &MainWindow::openFileInSelection);
    connect(searchOnInternet, &QAction::triggered, this, [=]() {
        const QString text = QString::fromUtf8(currentEditor()->getSelText()).trimmed();
        if (text.isEmpty()) return;

        const QString engine = app->getSettings()->value("Editor/SearchEngine", QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)")).toString();
        QString url = engine;
        url.replace(QLatin1String("$(CURRENT_WORD)"), QString::fromUtf8(QUrl::toPercentEncoding(text)));
        QDesktopServices::openUrl(QUrl(url));
    });
    connect(changeSearchEngine, &QAction::triggered, this, [=]() {
        const QStringList engines = {
            QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)"),
            QStringLiteral("https://duckduckgo.com/?q=$(CURRENT_WORD)"),
            QStringLiteral("https://www.bing.com/search?q=$(CURRENT_WORD)"),
            QStringLiteral("https://search.yahoo.com/search?p=$(CURRENT_WORD)"),
            QStringLiteral("https://stackoverflow.com/search?q=$(CURRENT_WORD)"),
        };
        const QString current = app->getSettings()->value("Editor/SearchEngine", engines.first()).toString();
        bool ok;
        const QString engine = QInputDialog::getItem(this, tr("Search Engine"), tr("URL ($(CURRENT_WORD) is replaced by the selection):"),
                                                     engines.contains(current) ? engines : QStringList(engines) << current,
                                                     engines.contains(current) ? engines.indexOf(current) : engines.size(), true, &ok);
        if (ok && !engine.isEmpty()) {
            app->getSettings()->setValue("Editor/SearchEngine", engine);
        }
    });
    menuOnSelection->addActions({openFileInSelection, searchOnInternet, changeSearchEngine});
    insertActionsAfter(ui->menuEdit, menuBlank->menuAction(), {menuOnSelection->menuAction()});

    // Panels and read-only
    ui->actionColumnMode->setText(tr("Column Editor..."));
    ui->actionColumnMode->setShortcut(QKeySequence(QStringLiteral("Alt+C")));

    QAction *setReadOnly = createAction(QStringLiteral("SetReadOnly"), tr("Read-Only"));
    setReadOnly->setCheckable(true);
    connect(setReadOnly, &QAction::triggered, this, [=](bool checked) {
        ScintillaNext *editor = currentEditor();
        editor->setReadOnly(checked);
        dockedEditor->refreshEditorIcon(editor);
    });

    QAction *clearReadOnlyFlag = createAction(QStringLiteral("ClearReadOnlyFlag"), tr("Clear Read-Only Flag"));
    connect(clearReadOnlyFlag, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();

        if (editor->isFile()) {
            QFile::setPermissions(editor->getFilePath(), QFile::permissions(editor->getFilePath()) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        }

        editor->setReadOnly(false);
        dockedEditor->refreshEditorIcon(editor);
        updateNppUi(editor);
    });

    ui->menuEdit->addAction(action(QStringLiteral("CharacterPanel")));
    ui->menuEdit->addAction(action(QStringLiteral("ClipboardHistory")));
    ui->menuEdit->addSeparator();
    ui->menuEdit->addActions({setReadOnly, clearReadOnlyFlag});
}

void MainWindow::insertDateTime(int format)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale locale = QLocale::system();
    QString text;

    switch (format) {
    case 0:
        text = locale.toString(now.time(), QLocale::ShortFormat) + QLatin1Char(' ') + locale.toString(now.date(), QLocale::ShortFormat);
        break;
    case 1:
        text = locale.toString(now.time(), QLocale::ShortFormat) + QLatin1Char(' ') + locale.toString(now.date(), QLocale::LongFormat);
        break;
    default:
        text = locale.toString(now, app->getSettings()->value("Editor/DateTimeFormat", QStringLiteral("yyyy-MM-dd HH:mm:ss")).toString());
        break;
    }

    currentEditor()->replaceSel(text.toUtf8().constData());
}

void MainWindow::openFileInSelection()
{
    ScintillaNext *editor = currentEditor();
    QString path = QString::fromUtf8(editor->getSelText()).trimmed();

    // Remove any quotes around the path
    if (path.size() > 1 && (path.startsWith(QLatin1Char('"')) || path.startsWith(QLatin1Char('\''))) && path.endsWith(path.at(0))) {
        path = path.mid(1, path.size() - 2);
    }

    if (path.isEmpty()) {
        return;
    }

    if (QDir::isRelativePath(path) && editor->isFile()) {
        path = editor->getFileInfo().dir().filePath(path);
    }

    if (QFileInfo(path).isFile()) {
        openFile(path);
    }
    else {
        QMessageBox::information(this, tr("Open File"), tr("<b>%1</b> does not exist.").arg(QDir::toNativeSeparators(path).toHtmlEscaped()));
    }
}

// ---------------------------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------------------------

void MainWindow::selectAndFind(bool forward, bool updateSearchText)
{
    ScintillaNext *editor = currentEditor();
    bool wholeWord = false;

    if (editor->selectionEmpty()) {
        const int pos = editor->currentPos();
        const int start = editor->wordStartPosition(pos, true);
        const int end = editor->wordEndPosition(pos, true);

        if (start == end) {
            return;
        }

        editor->setSelection(end, start);
        wholeWord = true;
    }

    const QByteArray text = editor->getSelText();

    if (updateSearchText) {
        FindReplaceDialog *frd = findChild<FindReplaceDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (frd) {
            frd->setFindString(QString::fromUtf8(text));
        }
    }

    const int flags = SCFIND_MATCHCASE | (wholeWord ? SCFIND_WHOLEWORD : 0);
    editor->setSearchFlags(flags);

    int found = -1;
    if (forward) {
        editor->setTargetRange(editor->selectionEnd(), editor->length());
        found = editor->searchInTarget(text.size(), text.constData());

        if (found == -1) {
            editor->setTargetRange(0, editor->selectionStart());
            found = editor->searchInTarget(text.size(), text.constData());
        }
    }
    else {
        editor->setTargetRange(editor->selectionStart(), 0);
        found = editor->searchInTarget(text.size(), text.constData());

        if (found == -1) {
            editor->setTargetRange(editor->length(), editor->selectionEnd());
            found = editor->searchInTarget(text.size(), text.constData());
        }
    }

    if (found != -1) {
        editor->goToRange({static_cast<Sci_PositionCR>(editor->targetStart()), static_cast<Sci_PositionCR>(editor->targetEnd())});
    }
}

void MainWindow::setupNppSearchMenu()
{
    // Find in Files
    ui->actionFindInFiles->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(ui->actionFindInFiles, &QAction::triggered, this, [=]() {
        FindInFilesDialog *dialog = findChild<FindInFilesDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (dialog == Q_NULLPTR) {
            dialog = new FindInFilesDialog(this);
        }

        dialog->setSearchResultsHandler(determineSearchResultsHandler());

        ScintillaNext *editor = currentEditor();
        if (!editor->selectionEmpty() && editor->lineFromPosition(editor->selectionStart()) == editor->lineFromPosition(editor->selectionEnd())) {
            dialog->setFindText(QString::fromUtf8(editor->getSelText()));
        }
        else {
            dialog->setFindText(QString());
        }

        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });

    // Select and Find / Volatile find
    QAction *selectFindNext = createAction(QStringLiteral("SelectAndFindNext"), tr("Select and Find Next"), QKeySequence(QStringLiteral("Ctrl+F3")));
    QAction *selectFindPrev = createAction(QStringLiteral("SelectAndFindPrevious"), tr("Select and Find Previous"), QKeySequence(QStringLiteral("Ctrl+Shift+F3")));
    QAction *volatileNext = createAction(QStringLiteral("FindVolatileNext"), tr("Find (Volatile) Next"), QKeySequence(QStringLiteral("Ctrl+Alt+F3")));
    QAction *volatilePrev = createAction(QStringLiteral("FindVolatilePrevious"), tr("Find (Volatile) Previous"), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+F3")));
    connect(selectFindNext, &QAction::triggered, this, [=]() { selectAndFind(true, true); });
    connect(selectFindPrev, &QAction::triggered, this, [=]() { selectAndFind(false, true); });
    connect(volatileNext, &QAction::triggered, this, [=]() { selectAndFind(true, false); });
    connect(volatilePrev, &QAction::triggered, this, [=]() { selectAndFind(false, false); });
    insertActionsAfter(ui->menuSearch, ui->actionFindPrevious, {selectFindNext, selectFindPrev, volatileNext, volatilePrev});

    // Braces & mark
    QAction *matchingBrace = createAction(QStringLiteral("GoToMatchingBrace"), tr("Go to Matching Brace"), QKeySequence(QStringLiteral("Ctrl+B")));
    QAction *selectBraces = createAction(QStringLiteral("SelectBetweenBraces"), tr("Select All In-between {} [] or ()"), QKeySequence(QStringLiteral("Ctrl+Alt+B")));
    QAction *mark = createAction(QStringLiteral("Mark"), tr("Mark..."), QKeySequence(QStringLiteral("Ctrl+M")));
    connect(matchingBrace, &QAction::triggered, this, [=]() { EditOperations::goToMatchingBrace(currentEditor(), false); });
    connect(selectBraces, &QAction::triggered, this, [=]() { EditOperations::goToMatchingBrace(currentEditor(), true); });
    connect(mark, &QAction::triggered, this, [=]() {
        MarkDialog *dialog = findChild<MarkDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (dialog == Q_NULLPTR) {
            dialog = new MarkDialog(this);
        }

        ScintillaNext *editor = currentEditor();
        QString text;
        if (!editor->selectionEmpty()) {
            text = QString::fromUtf8(editor->getSelText());
        }
        else {
            const int pos = editor->currentPos();
            text = QString::fromUtf8(editor->get_text_range(editor->wordStartPosition(pos, true), editor->wordEndPosition(pos, true)));
        }

        dialog->setFindText(text);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    insertActionsAfter(ui->menuSearch, ui->actionGoToLine, {matchingBrace, selectBraces, mark});

    // Two more mark styles, like Notepad++ has 5
    MarkerAppDecorator *markerAppDecorator = app->findChild<MarkerAppDecorator *>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 3; i < 5; ++i) {
        QAction *markStyle = createAction(QStringLiteral("MarkStyle%1").arg(i + 1), tr("Mark Style %1").arg(i + 1));
        QAction *clearStyle = createAction(QStringLiteral("ClearStyle%1").arg(i + 1), tr("Clear Style %1").arg(i + 1));

        if (markerAppDecorator) {
            QPixmap pixmap(16, 16);
            pixmap.fill(markerAppDecorator->markerColor(i));
            markStyle->setIcon(QIcon(pixmap));
            clearStyle->setIcon(QIcon(pixmap));
        }

        connect(markStyle, &QAction::triggered, this, [=]() {
            if (markerAppDecorator && markerAppDecorator->isEnabled()) markerAppDecorator->mark(currentEditor(), i);
        });
        connect(clearStyle, &QAction::triggered, this, [=]() {
            if (markerAppDecorator && markerAppDecorator->isEnabled()) markerAppDecorator->clear(currentEditor(), i);
        });

        ui->menuMarkAllOccurrences->addAction(markStyle);
        ui->menuClearMarks->insertAction(ui->actionClearAllStyles, clearStyle);
    }

    // Clearing all styles also clears the marks of the Mark dialog
    connect(ui->actionClearAllStyles, &QAction::triggered, this, [=]() { MarkDialog::clearMarks(currentEditor()); });

    // Jump up/down and copy styled text
    QMenu *menuJumpUp = new QMenu(tr("Jump Up"), this);
    QMenu *menuJumpDown = new QMenu(tr("Jump Down"), this);
    QMenu *menuCopyStyled = new QMenu(tr("Copy Styled Text"), this);

    auto jumpToFindMark = [=](bool forward) {
        ScintillaNext *editor = currentEditor();
        const int indicator = MarkDialog::markIndicator(editor);
        QList<QPair<int, int>> ranges;
        int pos = 0;
        while (pos < editor->length()) {
            const int end = editor->indicatorEnd(indicator, pos);
            if (editor->indicatorValueAt(indicator, pos)) ranges.append({pos, end});
            if (end <= pos) break;
            pos = end;
        }
        if (ranges.isEmpty()) return;

        QPair<int, int> target = forward ? ranges.first() : ranges.last();
        if (forward) {
            for (const auto &r : ranges) if (r.first >= editor->selectionEnd()) { target = r; break; }
        }
        else {
            for (auto it = ranges.crbegin(); it != ranges.crend(); ++it) if (it->second <= editor->selectionStart()) { target = *it; break; }
        }
        editor->goToRange({target.first, target.second});
    };

    const QStringList ordinals = {tr("1st Style"), tr("2nd Style"), tr("3rd Style"), tr("4th Style"), tr("5th Style")};
    for (int i = 0; i < 5; ++i) {
        QAction *up = createAction(QStringLiteral("JumpUpStyle%1").arg(i + 1), ordinals[i]);
        QAction *down = createAction(QStringLiteral("JumpDownStyle%1").arg(i + 1), ordinals[i]);
        QAction *copy = createAction(QStringLiteral("CopyStyledText%1").arg(i + 1), ordinals[i]);

        connect(up, &QAction::triggered, this, [=]() { if (markerAppDecorator) markerAppDecorator->jumpToMarked(currentEditor(), i, false); });
        connect(down, &QAction::triggered, this, [=]() { if (markerAppDecorator) markerAppDecorator->jumpToMarked(currentEditor(), i, true); });
        connect(copy, &QAction::triggered, this, [=]() {
            if (markerAppDecorator) {
                const QString text = markerAppDecorator->markedText(currentEditor(), i);
                if (!text.isEmpty()) QApplication::clipboard()->setText(text);
            }
        });

        menuJumpUp->addAction(up);
        menuJumpDown->addAction(down);
        menuCopyStyled->addAction(copy);
    }

    QAction *upFindMark = createAction(QStringLiteral("JumpUpFindMark"), tr("Find Mark Style"));
    QAction *downFindMark = createAction(QStringLiteral("JumpDownFindMark"), tr("Find Mark Style"));
    QAction *copyAllStyles = createAction(QStringLiteral("CopyStyledTextAll"), tr("All Styles"));
    QAction *copyFindMark = createAction(QStringLiteral("CopyStyledTextFindMark"), tr("Find Mark Style"));
    connect(upFindMark, &QAction::triggered, this, [=]() { jumpToFindMark(false); });
    connect(downFindMark, &QAction::triggered, this, [=]() { jumpToFindMark(true); });
    connect(copyAllStyles, &QAction::triggered, this, [=]() {
        if (markerAppDecorator) {
            const QString text = markerAppDecorator->markedText(currentEditor(), -1);
            if (!text.isEmpty()) QApplication::clipboard()->setText(text);
        }
    });
    connect(copyFindMark, &QAction::triggered, this, [=]() {
        const QString text = MarkDialog::markedText(currentEditor());
        if (!text.isEmpty()) QApplication::clipboard()->setText(text);
    });
    menuJumpUp->addAction(upFindMark);
    menuJumpDown->addAction(downFindMark);
    menuCopyStyled->addActions({copyAllStyles, copyFindMark});

    insertActionsAfter(ui->menuSearch, ui->menuClearMarks->menuAction(), {menuJumpUp->menuAction(), menuJumpDown->menuAction(), menuCopyStyled->menuAction()});

    // Bookmark extras
    QAction *removeUnmarked = createAction(QStringLiteral("RemoveNonBookmarkedLines"), tr("Remove Non-Bookmarked Lines"));
    QAction *pasteToBookmarked = createAction(QStringLiteral("PasteToBookmarkedLines"), tr("Paste to (Replace) Bookmarked Lines"));
    connect(removeUnmarked, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        BookMarkDecorator *bookmarks = editor->findChild<BookMarkDecorator *>(QString(), Qt::FindDirectChildrenOnly);
        if (bookmarks && bookmarks->isEnabled() && !bookmarks->bookMarkedLines().isEmpty()) {
            EditOperations::removeLinesNotMatching(editor, [=](int line) { return bookmarks->isBookmarkSet(line); });
        }
    });
    connect(pasteToBookmarked, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        BookMarkDecorator *bookmarks = editor->findChild<BookMarkDecorator *>(QString(), Qt::FindDirectChildrenOnly);
        const QByteArray text = QApplication::clipboard()->text().toUtf8();
        if (!bookmarks || !bookmarks->isEnabled()) return;

        QList<int> lines = bookmarks->bookMarkedLines();
        std::sort(lines.begin(), lines.end(), std::greater<int>());

        const UndoAction ua(editor);
        for (int line : lines) {
            editor->setTargetRange(editor->positionFromLine(line), editor->lineEndPosition(line));
            editor->replaceTarget(text.size(), text.constData());
        }
    });
    ui->menuBookmark->addSeparator();
    ui->menuBookmark->addActions({removeUnmarked, pasteToBookmarked});

    // Find characters in range
    QAction *findCharsInRange = createAction(QStringLiteral("FindCharactersInRange"), tr("Find Characters in Range..."));
    connect(findCharsInRange, &QAction::triggered, this, [=]() {
        FindCharactersInRangeDialog *dialog = findChild<FindCharactersInRangeDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (dialog == Q_NULLPTR) {
            dialog = new FindCharactersInRangeDialog(this);
        }
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    ui->menuSearch->addSeparator();
    ui->menuSearch->addAction(findCharsInRange);
}

// ---------------------------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------------------------

void MainWindow::togglePostIt(bool enable)
{
    if (enable == postItState.active) {
        return;
    }

    if (enable) {
        postItState.active = true;
        postItState.toolBarVisible = ui->mainToolBar->isVisible();
        postItState.statusBarVisible = ui->statusBar->isVisible();
        postItState.windowFlags = windowFlags();
        postItState.hiddenDocks.clear();

        for (QDockWidget *dock : findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (dock->isVisible()) {
                postItState.hiddenDocks.append(dock);
                dock->hide();
            }
        }

        // NOTE: don't hide() the menu bar as it will cancel the actions it holds
        ui->menuBar->setMaximumHeight(0);
        ui->mainToolBar->hide();
        ui->statusBar->hide();

        setWindowFlag(Qt::FramelessWindowHint, true);
        show();
        raise();
        activateWindow();
    }
    else {
        postItState.active = false;

        ui->menuBar->setMaximumHeight(app->getSettings()->showMenuBar() ? QWIDGETSIZE_MAX : 0);
        ui->mainToolBar->setVisible(postItState.toolBarVisible);
        ui->statusBar->setVisible(postItState.statusBarVisible);

        for (const QPointer<QWidget> &dock : postItState.hiddenDocks) {
            if (dock) dock->show();
        }

        setWindowFlags(postItState.windowFlags);
        show();
        raise();
        activateWindow();
    }

    currentEditor()->grabFocus();
}

void MainWindow::toggleDistractionFree(bool enable)
{
    if (enable == distractionFreeState.active) {
        return;
    }

    if (enable) {
        distractionFreeState.active = true;
        distractionFreeState.toolBarVisible = ui->mainToolBar->isVisible();
        distractionFreeState.statusBarVisible = ui->statusBar->isVisible();
        distractionFreeState.wasMaximized = isMaximized();
        distractionFreeState.wasFullScreen = isFullScreen();
        distractionFreeState.hiddenDocks.clear();

        for (QDockWidget *dock : findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (dock->isVisible()) {
                distractionFreeState.hiddenDocks.append(dock);
                dock->hide();
            }
        }

        ui->menuBar->setMaximumHeight(0);
        ui->mainToolBar->hide();
        ui->statusBar->hide();
        showFullScreen();

        if (exitDistractionFreeButton == Q_NULLPTR) {
            exitDistractionFreeButton = new QPushButton(QString(QChar(0x2715)) + QLatin1Char(' ') + tr("Exit Distraction Free Mode"), this);
            exitDistractionFreeButton->setCursor(Qt::PointingHandCursor);
            exitDistractionFreeButton->setShortcut(QKeySequence(Qt::Key_Escape));
            connect(exitDistractionFreeButton, &QPushButton::clicked, action(QStringLiteral("DistractionFreeMode")), &QAction::trigger);
        }

        exitDistractionFreeButton->adjustSize();
        exitDistractionFreeButton->show();
        exitDistractionFreeButton->raise();

        // Wait for the window to become full screen, then center the text in a column of about 100 characters
        QTimer::singleShot(100, this, [=]() {
            for (ScintillaNext *editor : editors()) {
                const int textWidth = editor->textWidth(STYLE_DEFAULT, "0123456789") * 10;
                const int margin = qMax(0, (editor->width() - textWidth) / 2);
                editor->setMarginLeft(margin);
                editor->setMarginRight(margin);
            }

            exitDistractionFreeButton->move(width() - exitDistractionFreeButton->width() - 40, 2);
        });
    }
    else {
        distractionFreeState.active = false;

        for (ScintillaNext *editor : editors()) {
            editor->setMarginLeft(2);
            editor->setMarginRight(1);
        }

        ui->menuBar->setMaximumHeight(app->getSettings()->showMenuBar() ? QWIDGETSIZE_MAX : 0);
        ui->mainToolBar->setVisible(distractionFreeState.toolBarVisible);
        ui->statusBar->setVisible(distractionFreeState.statusBarVisible);

        for (const QPointer<QWidget> &dock : distractionFreeState.hiddenDocks) {
            if (dock) dock->show();
        }

        if (exitDistractionFreeButton) {
            exitDistractionFreeButton->hide();
        }

        if (!distractionFreeState.wasFullScreen) {
            if (distractionFreeState.wasMaximized) showMaximized();
            else showNormal();
        }
    }

    currentEditor()->grabFocus();
}

void MainWindow::showSummary()
{
    ScintillaNext *editor = currentEditor();
    const QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(editor->characterPointer()), editor->textLength());
    const QString text = QString::fromUtf8(bytes);

    int characters = 0;
    for (const QChar c : text) {
        if (c != QLatin1Char('\n') && c != QLatin1Char('\r') && !c.isLowSurrogate()) {
            characters++;
        }
    }

    int selectedCharacters = 0;
    int selectedBytes = 0;
    int ranges = 0;
    for (int i = 0; i < editor->selections(); ++i) {
        const int start = editor->selectionNStart(i);
        const int end = editor->selectionNEnd(i);
        if (end > start) {
            selectedCharacters += editor->countCharacters(start, end);
            selectedBytes += end - start;
            ranges++;
        }
    }

    const QLocale locale;
    QString summary;

    if (editor->isFile()) {
        const QFileInfo info = editor->getFileInfo();
        summary += tr("Full file path: %1").arg(QDir::toNativeSeparators(editor->getFilePath())) + QLatin1Char('\n');
        summary += tr("Created: %1").arg(locale.toString(info.birthTime().isValid() ? info.birthTime() : info.metadataChangeTime(), QLocale::LongFormat)) + QLatin1Char('\n');
        summary += tr("Modified: %1").arg(locale.toString(info.lastModified(), QLocale::LongFormat)) + QLatin1Char('\n');
        summary += QLatin1Char('\n');
    }

    summary += tr("Characters (without line endings): %1").arg(locale.toString(characters)) + QLatin1Char('\n');
    summary += tr("Words: %1").arg(locale.toString(EditOperations::countWords(text))) + QLatin1Char('\n');
    summary += tr("Lines: %1").arg(locale.toString(static_cast<qlonglong>(editor->lineCount()))) + QLatin1Char('\n');
    summary += tr("Document length: %1").arg(locale.toString(static_cast<qlonglong>(editor->length()))) + QLatin1Char('\n');
    summary += tr("%1 selected characters (%2 bytes) in %3 ranges").arg(locale.toString(selectedCharacters), locale.toString(selectedBytes), locale.toString(ranges));

    QMessageBox::information(this, tr("Summary"), summary);
}

void MainWindow::setupNppViewMenu()
{
    // Always on top / Post-It / Distraction free
    QAction *alwaysOnTop = createAction(QStringLiteral("AlwaysOnTop"), tr("Always on Top"));
    alwaysOnTop->setCheckable(true);
    connect(alwaysOnTop, &QAction::toggled, this, [=](bool checked) {
        const bool visible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        if (visible) {
            show();
        }
        app->getSettings()->setValue("Gui/AlwaysOnTop", checked);
    });

    if (app->getSettings()->value("Gui/AlwaysOnTop", false).toBool()) {
        alwaysOnTop->setChecked(true);
    }

    QAction *postIt = createAction(QStringLiteral("PostIt"), tr("Post-It"), QKeySequence(Qt::Key_F12));
    postIt->setCheckable(true);
    connect(postIt, &QAction::toggled, this, &MainWindow::togglePostIt);

    QAction *distractionFree = createAction(QStringLiteral("DistractionFreeMode"), tr("Distraction Free Mode"));
    distractionFree->setCheckable(true);
    connect(distractionFree, &QAction::toggled, this, &MainWindow::toggleDistractionFree);

    ui->menuView->insertAction(ui->actionFullScreen, alwaysOnTop);
    insertActionsAfter(ui->menuView, ui->actionFullScreen, {postIt, distractionFree});

    // Move document / Tabs
    QMenu *menuMove = new QMenu(tr("Move/Clone Current Document"), this);
    QAction *moveToOtherView = createAction(QStringLiteral("MoveToOtherView"), tr("Move to Other View"));
    connect(moveToOtherView, &QAction::triggered, this, [=]() { dockedEditor->moveEditorToOtherView(currentEditor()); });
    menuMove->addAction(moveToOtherView);

    QMenu *menuTab = new QMenu(tr("Tab"), this);

    // Switches to the tab at the index of the current view, a negative index means the last tab
    auto goToTab = [=](int index) {
        ads::CDockAreaWidget *area = dockedEditor->currentDockArea();
        if (area == Q_NULLPTR || area->dockWidgetsCount() == 0) {
            return;
        }

        if (index < 0) {
            index = area->dockWidgetsCount() - 1;
        }

        if (index < area->dockWidgetsCount()) {
            ScintillaNext *editor = qobject_cast<ScintillaNext *>(area->dockWidget(index)->widget());
            if (editor) {
                dockedEditor->switchToEditor(editor);
                editor->grabFocus();
            }
        }
    };

    // Like web browsers: Cmd/Ctrl+1 to Cmd/Ctrl+8 switch to that tab, Cmd/Ctrl+9 switches to the last tab.
    // The numeric keypad works like Notepad++ (Ctrl+NumPad 1-9 switch to tab 1-9).
    const QStringList tabNames = {tr("1st Tab"), tr("2nd Tab"), tr("3rd Tab"), tr("4th Tab"), tr("5th Tab"), tr("6th Tab"), tr("7th Tab"), tr("8th Tab"), tr("9th Tab")};
    for (int i = 0; i < 9; ++i) {
        const Qt::Key key = static_cast<Qt::Key>(Qt::Key_1 + i);
        QAction *tab = createAction(QStringLiteral("GoToTab%1").arg(i + 1), tabNames[i]);

        QList<QKeySequence> shortcuts;
        if (i < 8) {
            shortcuts.append(QKeySequence(Qt::CTRL | key));
        }
        shortcuts.append(QKeySequence(Qt::CTRL | Qt::KeypadModifier | key));
        tab->setShortcuts(shortcuts);

        connect(tab, &QAction::triggered, this, [=]() { goToTab(i); });
        menuTab->addAction(tab);
    }

    QAction *lastTab = createAction(QStringLiteral("GoToLastTab"), tr("Last Tab"), QKeySequence(Qt::CTRL | Qt::Key_9));
    connect(lastTab, &QAction::triggered, this, [=]() { goToTab(-1); });
    menuTab->addAction(lastTab);
    menuTab->addSeparator();

    ui->actionNextTab->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Tab")), QKeySequence(QStringLiteral("Ctrl+PgDown"))});
    ui->actionPreviousTab->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Shift+Tab")), QKeySequence(QStringLiteral("Ctrl+PgUp"))});
    menuTab->addActions({ui->actionNextTab, ui->actionPreviousTab});
    menuTab->addSeparator();

    QAction *moveTabForward = createAction(QStringLiteral("MoveTabForward"), tr("Move Tab Forward"), QKeySequence(QStringLiteral("Ctrl+Shift+PgDown")));
    QAction *moveTabBackward = createAction(QStringLiteral("MoveTabBackward"), tr("Move Tab Backward"), QKeySequence(QStringLiteral("Ctrl+Shift+PgUp")));
    connect(moveTabForward, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        const int index = dockedEditor->editorsInSameView(editor).indexOf(editor);
        dockedEditor->moveEditorToIndex(editor, index + 1);
        editor->grabFocus();
    });
    connect(moveTabBackward, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        const int index = dockedEditor->editorsInSameView(editor).indexOf(editor);
        dockedEditor->moveEditorToIndex(editor, index - 1);
        editor->grabFocus();
    });
    menuTab->addActions({moveTabForward, moveTabBackward});

    insertActionsAfter(ui->menuView, ui->menuZoom->menuAction(), {menuMove->menuAction(), menuTab->menuAction()});

    // Focus on another view / hide lines
    QAction *focusOtherView = createAction(QStringLiteral("FocusOnAnotherView"), tr("Focus on Another View"), QKeySequence(Qt::Key_F8));
    connect(focusOtherView, &QAction::triggered, this, [=]() {
        ScintillaNext *other = dockedEditor->editorInOtherView(currentEditor());
        if (other) {
            dockedEditor->switchToEditor(other);
            other->grabFocus();
        }
    });

    QAction *hideLines = createAction(QStringLiteral("HideLines"), tr("Hide Lines"), QKeySequence(QStringLiteral("Alt+H")));
    connect(hideLines, &QAction::triggered, this, [=]() {
        HideLinesDecorator *decorator = currentEditor()->findChild<HideLinesDecorator *>(QString(), Qt::FindDirectChildrenOnly);
        if (decorator) decorator->hideSelectedLines();
    });

    QAction *showHiddenLines = createAction(QStringLiteral("ShowAllHiddenLines"), tr("Show All Hidden Lines"));
    connect(showHiddenLines, &QAction::triggered, this, [=]() {
        HideLinesDecorator *decorator = currentEditor()->findChild<HideLinesDecorator *>(QString(), Qt::FindDirectChildrenOnly);
        if (decorator) decorator->showAllHiddenLines();
    });

    insertActionsAfter(ui->menuView, ui->actionWordWrap, {focusOtherView, hideLines, showHiddenLines});

    // Folding
    QAction *foldCurrent = createAction(QStringLiteral("FoldCurrentLevel"), tr("Fold Current Level"), QKeySequence(QStringLiteral("Ctrl+Alt+F")));
    QAction *unfoldCurrent = createAction(QStringLiteral("UnfoldCurrentLevel"), tr("Unfold Current Level"), QKeySequence(QStringLiteral("Ctrl+Alt+Shift+F")));
    connect(foldCurrent, &QAction::triggered, this, [=]() { EditOperations::foldCurrentLevel(currentEditor(), true); });
    connect(unfoldCurrent, &QAction::triggered, this, [=]() { EditOperations::foldCurrentLevel(currentEditor(), false); });
    insertActionsAfter(ui->menuView, ui->actionUnfoldAll, {foldCurrent, unfoldCurrent});

    // Summary
    QAction *summary = createAction(QStringLiteral("Summary"), tr("Summary..."));
    connect(summary, &QAction::triggered, this, &MainWindow::showSummary);
    insertActionsAfter(ui->menuView, ui->menuUnfold_Level->menuAction(), {createSeparator(this), summary});

    // Panels
    ui->menuView->addAction(action(QStringLiteral("DocumentMap")));
    ui->menuView->addAction(action(QStringLiteral("FunctionList")));
    ui->menuView->addSeparator();

    // Synchronized scrolling
    QAction *syncVertical = createAction(QStringLiteral("SynchronizeVerticalScrolling"), tr("Synchronize Vertical Scrolling"));
    QAction *syncHorizontal = createAction(QStringLiteral("SynchronizeHorizontalScrolling"), tr("Synchronize Horizontal Scrolling"));
    syncVertical->setCheckable(true);
    syncHorizontal->setCheckable(true);
    connect(syncVertical, &QAction::toggled, this, [=](bool checked) {
        scrollSynchronizer->setEditors(currentEditor(), dockedEditor->editorInOtherView(currentEditor()));
        scrollSynchronizer->setVertical(checked);
    });
    connect(syncHorizontal, &QAction::toggled, this, [=](bool checked) {
        scrollSynchronizer->setEditors(currentEditor(), dockedEditor->editorInOtherView(currentEditor()));
        scrollSynchronizer->setHorizontal(checked);
    });
    ui->menuView->addActions({syncVertical, syncHorizontal});
    ui->menuView->addSeparator();

    // Monitoring
    QAction *monitoring = createAction(QStringLiteral("Monitoring"), tr("Monitoring (tail -f)"));
    monitoring->setCheckable(true);
    connect(monitoring, &QAction::triggered, this, [=](bool checked) {
        ScintillaNext *editor = currentEditor();

        if (checked && !editor->isFile()) {
            monitoring->setChecked(false);
            return;
        }

        if (checked && !editor->isSavedToDisk()) {
            const auto reply = QMessageBox::question(this, tr("Monitoring"), tr("<b>%1</b> has unsaved changes. Do you want to save it before monitoring it?").arg(editor->getName()), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (reply == QMessageBox::Cancel || (reply == QMessageBox::Save && !saveFile(editor))) {
                monitoring->setChecked(false);
                return;
            }
        }

        fileMonitor->setMonitored(editor, checked);
    });
    ui->menuView->addAction(monitoring);
}

// ---------------------------------------------------------------------------------------------
// Encoding
// ---------------------------------------------------------------------------------------------

void MainWindow::setupNppEncodingMenu()
{
    QMenu *menu = ui->menuEncodings;
    menu->clear();

    encodingActionGroup = new QActionGroup(this);
    encodingActionGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    const int NoBom = static_cast<int>(ScintillaNext::BomType::None);

    auto addEncodingAction = [=](QMenu *target, const QString &name, const QString &text, const QByteArray &encoding, int bom, bool convert) {
        QAction *a = name.isEmpty() ? new QAction(text, this) : createAction(name, text);
        a->setData(QVariant::fromValue(EncodingChoice{encoding, bom}));

        if (!convert) {
            a->setCheckable(true);
            encodingActionGroup->addAction(a);
        }

        connect(a, &QAction::triggered, this, [=]() {
            changeEncoding(encoding, static_cast<ScintillaNext::BomType>(bom), convert);
        });

        target->addAction(a);
        return a;
    };

    addEncodingAction(menu, QStringLiteral("EncodeAnsi"), tr("ANSI"), TextEncoding::ansiCodecName(), NoBom, false);
    addEncodingAction(menu, QStringLiteral("EncodeUtf8"), tr("UTF-8"), TextEncoding::UTF8, NoBom, false);
    addEncodingAction(menu, QStringLiteral("EncodeUtf8Bom"), tr("UTF-8-BOM"), TextEncoding::UTF8, static_cast<int>(ScintillaNext::BomType::Utf8), false);
    addEncodingAction(menu, QStringLiteral("EncodeUtf16BeBom"), tr("UTF-16 BE BOM"), TextEncoding::UTF16BE, static_cast<int>(ScintillaNext::BomType::Utf16BE), false);
    addEncodingAction(menu, QStringLiteral("EncodeUtf16LeBom"), tr("UTF-16 LE BOM"), TextEncoding::UTF16LE, static_cast<int>(ScintillaNext::BomType::Utf16LE), false);

    QMenu *menuCharacterSets = menu->addMenu(tr("Character Sets"));
    for (const TextEncoding::CharacterSetGroup &group : TextEncoding::characterSetGroups()) {
        QMenu *groupMenu = Q_NULLPTR;

        for (const TextEncoding::CharacterSet &set : group.sets) {
            if (!TextEncoding::isAvailable(set.codecName)) {
                continue;
            }

            if (groupMenu == Q_NULLPTR) {
                groupMenu = menuCharacterSets->addMenu(group.name);
            }

            addEncodingAction(groupMenu, QString(), set.displayName, set.codecName, NoBom, false);
        }
    }

    menu->addSeparator();

    addEncodingAction(menu, QStringLiteral("ConvertToAnsi"), tr("Convert to ANSI"), TextEncoding::ansiCodecName(), NoBom, true);
    addEncodingAction(menu, QStringLiteral("ConvertToUtf8"), tr("Convert to UTF-8"), TextEncoding::UTF8, NoBom, true);
    addEncodingAction(menu, QStringLiteral("ConvertToUtf8Bom"), tr("Convert to UTF-8-BOM"), TextEncoding::UTF8, static_cast<int>(ScintillaNext::BomType::Utf8), true);
    addEncodingAction(menu, QStringLiteral("ConvertToUtf16BeBom"), tr("Convert to UTF-16 BE BOM"), TextEncoding::UTF16BE, static_cast<int>(ScintillaNext::BomType::Utf16BE), true);
    addEncodingAction(menu, QStringLiteral("ConvertToUtf16LeBom"), tr("Convert to UTF-16 LE BOM"), TextEncoding::UTF16LE, static_cast<int>(ScintillaNext::BomType::Utf16LE), true);

    // Clicking the encoding in the status bar shows this menu (like the EOL format)
    connect(ui->statusBar, &EditorInfoStatusBar::customContextMenuRequestedForEncodingLabel, this, [=](const QPoint &pos) {
        ui->menuEncodings->popup(pos);
    });
    connect(ui->statusBar, &EditorInfoStatusBar::customContextMenuRequestedForLanguageLabel, this, [=](const QPoint &pos) {
        ui->menuLanguage->popup(pos);
    });
}

void MainWindow::changeEncoding(const QByteArray &encoding, ScintillaNext::BomType bom, bool convert)
{
    ScintillaNext *editor = currentEditor();
    const QString name = TextEncoding::displayName(encoding);

    if (convert) {
        // Make sure nothing gets lost
        QTextCodec *codec = TextEncoding::codecForName(encoding);
        if (codec && !TextEncoding::isUtf8(encoding)) {
            const QString text = QString::fromUtf8(reinterpret_cast<const char *>(editor->characterPointer()), editor->textLength());

            if (!codec->canEncode(text)) {
                const auto reply = QMessageBox::warning(this, tr("Convert Encoding"),
                                                        tr("Some characters of this document can't be represented in <b>%1</b> and will be replaced when saving.<br><br>Do you want to continue?").arg(name),
                                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (reply != QMessageBox::Yes) {
                    updateEncodingBasedUi(editor);
                    return;
                }
            }
        }

        editor->setEncoding(encoding, bom, true);
    }
    else if (editor->isFile() && QFile::exists(editor->getFilePath())) {
        if (!editor->isSavedToDisk()) {
            const auto reply = QMessageBox::question(this, tr("Change Encoding"),
                                                     tr("The document has unsaved changes. Reloading it as <b>%1</b> will lose them.<br><br>Do you want to continue?").arg(name));
            if (reply != QMessageBox::Yes) {
                updateEncodingBasedUi(editor);
                return;
            }
        }

        editor->reloadWithEncoding(encoding);

        // e.g. "UTF-8-BOM" was selected for a file without a BOM, it needs saved to add it
        if (editor->bom() != bom) {
            editor->setEncoding(editor->encoding(), bom, true);
        }
    }
    else {
        editor->setEncoding(encoding, bom, false);
    }

    updateEncodingBasedUi(editor);
    ui->statusBar->refresh(editor);
}

void MainWindow::updateEncodingBasedUi(ScintillaNext *editor)
{
    if (encodingActionGroup == Q_NULLPTR) {
        return;
    }

    const QByteArray canonical = TextEncoding::isUtf8(editor->encoding()) ? TextEncoding::UTF8 : TextEncoding::canonicalName(editor->encoding());
    const int bom = static_cast<int>(editor->bom());
    QAction *match = Q_NULLPTR;

    for (QAction *a : encodingActionGroup->actions()) {
        const EncodingChoice choice = a->data().value<EncodingChoice>();
        const QByteArray choiceCanonical = TextEncoding::isUtf8(choice.encoding) ? TextEncoding::UTF8 : TextEncoding::canonicalName(choice.encoding);

        // UTF-16 is always shown as having a BOM
        const bool bomMatches = choice.bom == bom || choiceCanonical == TextEncoding::canonicalName(TextEncoding::UTF16LE) || choiceCanonical == TextEncoding::canonicalName(TextEncoding::UTF16BE);

        if (choiceCanonical == canonical && bomMatches) {
            match = a;
            break;
        }
    }

    if (match) {
        match->setChecked(true);
    }
    else if (encodingActionGroup->checkedAction()) {
        encodingActionGroup->checkedAction()->setChecked(false);
    }
}

void MainWindow::updateNppUi(ScintillaNext *editor)
{
    updateEncodingBasedUi(editor);

    if (QAction *a = action(QStringLiteral("SetReadOnly"))) {
        a->setChecked(editor->readOnly());
    }

    const bool isFile = editor->isFile();

    if (QAction *a = action(QStringLiteral("ClearReadOnlyFlag"))) {
        a->setEnabled(isFile && !QFileInfo(editor->getFilePath()).isWritable());
    }

    if (QAction *a = action(QStringLiteral("OpenInDefaultViewer"))) {
        a->setEnabled(isFile);
    }

    if (QAction *a = action(QStringLiteral("Monitoring"))) {
        a->setEnabled(isFile);
        a->setChecked(fileMonitor && fileMonitor->isMonitored(editor));
    }

    if (QAction *a = action(QStringLiteral("MoveToOtherView"))) {
        a->setEnabled(editorCount() > 1);
    }
}

// ---------------------------------------------------------------------------------------------
// Settings, Tools, Macro, Run, Window, Help
// ---------------------------------------------------------------------------------------------

void MainWindow::setupNppSettingsMenu()
{
    QAction *shortcutMapper = createAction(QStringLiteral("ShortcutMapper"), tr("Shortcut Mapper..."));
    connect(shortcutMapper, &QAction::triggered, this, [=]() {
        ShortcutMapperDialog *dialog = findChild<ShortcutMapperDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (dialog == Q_NULLPTR) {
            dialog = new ShortcutMapperDialog(this, ui->menuBar, app->getSettings(), this);
        }
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });

    ui->menuSettings->addAction(shortcutMapper);
}

void MainWindow::setupNppToolsMenu()
{
    QMenu *menuTools = new QMenu(tr("&Tools"), this);
    menuTools->setObjectName(QStringLiteral("menuTools"));

    const QList<QPair<QCryptographicHash::Algorithm, QString>> algorithms = {
        {QCryptographicHash::Md5, QStringLiteral("Md5")},
        {QCryptographicHash::Sha1, QStringLiteral("Sha1")},
        {QCryptographicHash::Sha256, QStringLiteral("Sha256")},
        {QCryptographicHash::Sha512, QStringLiteral("Sha512")},
    };

    for (const auto &algorithm : algorithms) {
        const QCryptographicHash::Algorithm algo = algorithm.first;
        QMenu *submenu = menuTools->addMenu(HashGeneratorDialog::algorithmName(algo));

        QAction *generate = createAction(algorithm.second + QStringLiteral("Generate"), tr("Generate..."));
        QAction *fromFiles = createAction(algorithm.second + QStringLiteral("GenerateFromFiles"), tr("Generate from files..."));
        QAction *fromSelection = createAction(algorithm.second + QStringLiteral("GenerateFromSelection"), tr("Generate from selection into clipboard"));

        connect(generate, &QAction::triggered, this, [=]() {
            (new HashGeneratorDialog(algo, HashGeneratorDialog::Mode::Text, this))->show();
        });
        connect(fromFiles, &QAction::triggered, this, [=]() {
            (new HashGeneratorDialog(algo, HashGeneratorDialog::Mode::Files, this))->show();
        });
        connect(fromSelection, &QAction::triggered, this, [=]() {
            ScintillaNext *editor = currentEditor();
            QStringList hashes;

            for (int i = 0; i < editor->selections(); ++i) {
                const int start = editor->selectionNStart(i);
                const int end = editor->selectionNEnd(i);

                if (end > start) {
                    hashes.append(HashGeneratorDialog::hash(editor->get_text_range(start, end), algo));
                }
            }

            if (!hashes.isEmpty()) {
                QApplication::clipboard()->setText(hashes.join(QLatin1Char('\n')));
                FadingIndicator::showText(editor, tr("%1 copied to clipboard").arg(HashGeneratorDialog::algorithmName(algo)));
            }
        });

        submenu->addActions({generate, fromFiles, fromSelection});
    }

    ui->menuBar->insertMenu(ui->menuMacro->menuAction(), menuTools);
}

void MainWindow::setupNppMacroMenu()
{
    // Notepad++ ships with this macro by default
    QAction *trimAndSave = createAction(QStringLiteral("TrimTrailingSpaceAndSave"), tr("Trim Trailing Space and Save"), QKeySequence(QStringLiteral("Alt+Shift+S")));
    connect(trimAndSave, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();

        // Only the trailing space of the whole document
        const int caret = editor->currentPos();
        editor->setEmptySelection(caret);
        EditOperations::trimTrailingSpace(editor);
        editor->gotoPos(qMin(caret, static_cast<int>(editor->lineEndPosition(editor->lineFromPosition(caret)))));

        saveCurrentFile();
    });

    // Put it before the list of saved macros
    QAction *firstSeparator = Q_NULLPTR;
    for (QAction *a : ui->menuMacro->actions()) {
        if (a->isSeparator()) {
            firstSeparator = a;
            break;
        }
    }

    if (firstSeparator) {
        ui->menuMacro->insertAction(firstSeparator, trimAndSave);
    }
    else {
        ui->menuMacro->addAction(trimAndSave);
    }
}

void MainWindow::setupNppRunMenu()
{
    QMenu *menuRun = new QMenu(tr("&Run"), this);
    menuRun->setObjectName(QStringLiteral("menuRun"));

    QAction *run = createAction(QStringLiteral("Run"), tr("Run..."), QKeySequence(Qt::Key_F5));
    connect(run, &QAction::triggered, this, [=]() {
        RunDialog dialog(app->getSettings(), currentEditor(), this);
        dialog.exec();
    });

    QAction *launchInBrowser = createAction(QStringLiteral("LaunchInBrowser"), tr("Launch in Default Browser"));
    connect(launchInBrowser, &QAction::triggered, this, [=]() {
        ScintillaNext *editor = currentEditor();
        if (editor->isFile()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(editor->getFilePath()));
        }
    });

    QAction *manage = createAction(QStringLiteral("ManageRunCommands"), tr("Delete Saved Command..."));
    connect(manage, &QAction::triggered, this, [=]() {
        QList<QPair<QString, QString>> commands = RunDialog::savedCommands(app->getSettings());
        if (commands.isEmpty()) return;

        QStringList names;
        for (const auto &command : commands) names.append(command.first);

        bool ok;
        const QString name = QInputDialog::getItem(this, tr("Delete Saved Command"), tr("Command:"), names, 0, false, &ok);
        if (ok) {
            commands.removeAt(names.indexOf(name));
            RunDialog::setSavedCommands(app->getSettings(), commands);
        }
    });

    menuRun->addAction(run);
    menuRun->addSeparator();
    menuRun->addAction(launchInBrowser);
    menuRun->addSeparator();
    QAction *manageSeparator = menuRun->addSeparator();
    menuRun->addAction(manage);

    connect(menuRun, &QMenu::aboutToShow, this, [=]() {
        for (QAction *a : runMenuDynamicActions) {
            menuRun->removeAction(a);
            delete a;
        }
        runMenuDynamicActions.clear();

        launchInBrowser->setEnabled(currentEditor()->isFile());

        for (const auto &command : RunDialog::savedCommands(app->getSettings())) {
            QAction *a = new QAction(command.first, menuRun);
            const QString cmd = command.second;
            connect(a, &QAction::triggered, this, [=]() {
                if (!RunDialog::execute(cmd, currentEditor())) {
                    QMessageBox::warning(this, tr("Run"), tr("Unable to run <b>%1</b>").arg(cmd.toHtmlEscaped()));
                }
            });
            menuRun->insertAction(manageSeparator, a);
            runMenuDynamicActions.append(a);
        }
    });

    ui->menuBar->insertMenu(ui->menuHelp->menuAction(), menuRun);
}

void MainWindow::sortTabs(int column, bool ascending)
{
    ScintillaNext *current = currentEditor();
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    auto key = [&](ScintillaNext *editor) -> QString {
        switch (column) {
        case 0: return editor->getName();
        case 1: return editor->isFile() ? editor->getFilePath() : editor->getName();
        case 2: return editor->languageName;
        default: return QStringLiteral("%1").arg(editor->length(), 15, 10, QLatin1Char('0'));
        }
    };

    QSet<ScintillaNext *> handled;
    for (ScintillaNext *editor : editors()) {
        if (handled.contains(editor)) continue;

        QVector<ScintillaNext *> view = dockedEditor->editorsInSameView(editor);
        std::stable_sort(view.begin(), view.end(), [&](ScintillaNext *a, ScintillaNext *b) {
            const int result = collator.compare(key(a), key(b));
            return ascending ? result < 0 : result > 0;
        });

        for (int i = 0; i < view.size(); ++i) {
            dockedEditor->moveEditorToIndex(view[i], i);
            handled.insert(view[i]);
        }
    }

    dockedEditor->switchToEditor(current);
}

void MainWindow::setupNppWindowMenu()
{
    QMenu *menuWindow = new QMenu(tr("&Window"), this);
    menuWindow->setObjectName(QStringLiteral("menuWindow"));

    QAction *windows = createAction(QStringLiteral("Windows"), tr("Windows..."));
    connect(windows, &QAction::triggered, this, [=]() {
        WindowsDialog *dialog = findChild<WindowsDialog *>(QString(), Qt::FindDirectChildrenOnly);
        if (dialog == Q_NULLPTR) {
            dialog = new WindowsDialog(this);
        }
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });

    QMenu *menuSortBy = new QMenu(tr("Sort By"), this);
    const QList<QPair<QString, QPair<int, bool>>> sorts = {
        {tr("Name A to Z"), {0, true}}, {tr("Name Z to A"), {0, false}},
        {tr("Path A to Z"), {1, true}}, {tr("Path Z to A"), {1, false}},
        {tr("Type A to Z"), {2, true}}, {tr("Type Z to A"), {2, false}},
        {tr("Content Length Ascending"), {3, true}}, {tr("Content Length Descending"), {3, false}},
    };
    for (const auto &sort : sorts) {
        const int column = sort.second.first;
        const bool ascending = sort.second.second;
        menuSortBy->addAction(sort.first, this, [=]() { sortTabs(column, ascending); });
    }

    connect(menuWindow, &QMenu::aboutToShow, this, [=]() {
        menuWindow->clear();
        menuWindow->addAction(windows);
        menuWindow->addMenu(menuSortBy);
        menuWindow->addSeparator();

        ScintillaNext *current = currentEditor();
        int i = 1;

        for (ScintillaNext *editor : editors()) {
            QString text = editor->getName();
            if (!editor->isSavedToDisk()) text += QStringLiteral(" *");
            text.replace(QLatin1Char('&'), QLatin1String("&&"));

            if (i < 10) text = QStringLiteral("&%1: %2").arg(i).arg(text);
            else text = QStringLiteral("%1: %2").arg(i).arg(text);

            QAction *a = menuWindow->addAction(text);
            a->setCheckable(true);
            a->setChecked(editor == current);
            QPointer<ScintillaNext> pointer = editor;
            connect(a, &QAction::triggered, this, [=]() {
                if (pointer) {
                    dockedEditor->switchToEditor(pointer);
                    pointer->grabFocus();
                }
            });
            i++;
        }
    });

    // Needs at least one entry so the menu can be shown on all platforms
    menuWindow->addAction(windows);

    ui->menuBar->insertMenu(ui->menuHelp->menuAction(), menuWindow);
}

void MainWindow::setupNppHelpMenu()
{
    QAction *commandLine = createAction(QStringLiteral("CommandLineArguments"), tr("Command Line Arguments..."));
    connect(commandLine, &QAction::triggered, this, [=]() {
        const QString text = QStringLiteral(
            "NotepadNext [options] [files...]\n\n"
            "-h, --help                     Displays help on commandline options.\n"
            "-v, --version                  Displays version information.\n"
            "--translation <translation>    Overrides the system default translation.\n"
            "--reset-settings               Resets all application settings.\n"
            "-n <line number>               Places the cursor on the line number for the first file specified.\n"
            "--workspace <workspace path>   Opens the specified folder as a workspace.\n");

        QMessageBox box(QMessageBox::Information, tr("Command Line Arguments"), text, QMessageBox::Ok, this);
        box.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        box.setTextInteractionFlags(Qt::TextSelectableByMouse);
        box.exec();
    });

    ui->menuHelp->insertAction(ui->actionCheckForUpdates, commandLine);
}
