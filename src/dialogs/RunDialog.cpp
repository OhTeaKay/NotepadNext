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

#include "RunDialog.h"

#include "ApplicationSettings.h"
#include "ScintillaNext.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>
#include <QVBoxLayout>


RunDialog::RunDialog(ApplicationSettings *settings, ScintillaNext *editor, QWidget *parent) :
    QDialog(parent, Qt::Dialog),
    settings(settings),
    editor(editor)
{
    setWindowTitle(tr("Run..."));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(500);

    comboCommand = new QComboBox();
    comboCommand->setEditable(true);
    comboCommand->setInsertPolicy(QComboBox::NoInsert);
    comboCommand->addItems(settings->value("Run/History").toStringList());
    comboCommand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QPushButton *buttonBrowse = new QPushButton(QStringLiteral("..."));
    buttonBrowse->setMaximumWidth(40);
    connect(buttonBrowse, &QPushButton::clicked, this, &RunDialog::browse);

    QHBoxLayout *commandLayout = new QHBoxLayout();
    commandLayout->addWidget(comboCommand);
    commandLayout->addWidget(buttonBrowse);

    QLabel *help = new QLabel(tr("<small>Variables: $(FULL_CURRENT_PATH), $(CURRENT_DIRECTORY), $(FILE_NAME), $(NAME_PART), "
                                 "$(EXT_PART), $(CURRENT_WORD), $(CURRENT_LINE), $(CURRENT_LINESTR), $(CURRENT_COLUMN), "
                                 "$(NPP_DIRECTORY), $(NPP_FULL_FILE_PATH), $(SYS.&lt;ENV_VAR&gt;)</small>"));
    help->setWordWrap(true);

    QPushButton *buttonRun = new QPushButton(tr("Run"));
    QPushButton *buttonSave = new QPushButton(tr("Save..."));
    QPushButton *buttonCancel = new QPushButton(tr("Cancel"));
    buttonRun->setDefault(true);

    connect(buttonRun, &QPushButton::clicked, this, &RunDialog::run);
    connect(buttonSave, &QPushButton::clicked, this, &RunDialog::save);
    connect(buttonCancel, &QPushButton::clicked, this, &QDialog::reject);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(buttonRun);
    buttons->addWidget(buttonSave);
    buttons->addWidget(buttonCancel);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("The Program to Run")));
    layout->addLayout(commandLayout);
    layout->addWidget(help);
    layout->addLayout(buttons);
}

QList<QPair<QString, QString>> RunDialog::savedCommands(ApplicationSettings *settings)
{
    QList<QPair<QString, QString>> commands;

    if (!settings->contains("Run/SavedCommands")) {
        // Some useful defaults, similar to the ones Notepad++ ships with
        commands.append({tr("Search on Google"), QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)")});
        commands.append({tr("Wikipedia Search"), QStringLiteral("https://en.wikipedia.org/wiki/Special:Search?search=$(CURRENT_WORD)")});
        commands.append({tr("Open Containing Folder"), QStringLiteral("$(CURRENT_DIRECTORY)")});
        return commands;
    }

    const QVariantList list = settings->value("Run/SavedCommands").toList();
    for (const QVariant &v : list) {
        const QStringList pair = v.toStringList();

        if (pair.size() == 2) {
            commands.append({pair.at(0), pair.at(1)});
        }
    }

    return commands;
}

void RunDialog::setSavedCommands(ApplicationSettings *settings, const QList<QPair<QString, QString>> &commands)
{
    QVariantList list;

    for (const auto &command : commands) {
        list.append(QStringList{command.first, command.second});
    }

    settings->setValue("Run/SavedCommands", list);
}

QString RunDialog::expandVariables(const QString &command, ScintillaNext *editor, bool urlEncode)
{
    QHash<QString, QString> variables;

    if (editor) {
        const QFileInfo info = editor->getFileInfo();
        const bool isFile = editor->isFile();

        variables[QStringLiteral("FULL_CURRENT_PATH")] = isFile ? QDir::toNativeSeparators(editor->getFilePath()) : editor->getName();
        variables[QStringLiteral("CURRENT_DIRECTORY")] = isFile ? QDir::toNativeSeparators(editor->getPath()) : QDir::toNativeSeparators(QDir::currentPath());
        variables[QStringLiteral("FILE_NAME")] = isFile ? info.fileName() : editor->getName();
        variables[QStringLiteral("NAME_PART")] = isFile ? info.completeBaseName() : editor->getName();
        variables[QStringLiteral("EXT_PART")] = isFile && !info.suffix().isEmpty() ? QStringLiteral(".") + info.suffix() : QString();

        // The current word is the selection, or the word at the caret
        QByteArray word;
        if (!editor->selectionEmpty()) {
            word = editor->getSelText();
        }
        else {
            const int pos = editor->currentPos();
            word = editor->get_text_range(editor->wordStartPosition(pos, true), editor->wordEndPosition(pos, true));
        }
        variables[QStringLiteral("CURRENT_WORD")] = QString::fromUtf8(word);

        const int pos = editor->currentPos();
        const int line = editor->lineFromPosition(pos);
        variables[QStringLiteral("CURRENT_LINE")] = QString::number(line + 1);
        variables[QStringLiteral("CURRENT_COLUMN")] = QString::number(editor->column(pos) + 1);
        variables[QStringLiteral("CURRENT_LINESTR")] = QString::fromUtf8(editor->get_text_range(editor->positionFromLine(line), editor->lineEndPosition(line)));
    }

    variables[QStringLiteral("NPP_DIRECTORY")] = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    variables[QStringLiteral("NPP_FULL_FILE_PATH")] = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    QString result;
    static const QRegularExpression re(QStringLiteral("\\$\\(([A-Za-z_\\.]+[A-Za-z0-9_]*)\\)"));
    int last = 0;

    QRegularExpressionMatchIterator it = re.globalMatch(command);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        QString value;

        if (name.startsWith(QLatin1String("SYS."))) {
            value = QProcessEnvironment::systemEnvironment().value(name.mid(4));
        }
        else if (variables.contains(name)) {
            value = variables.value(name);
        }
        else {
            value = match.captured(0);
        }

        if (urlEncode) {
            value = QString::fromUtf8(QUrl::toPercentEncoding(value));
        }

        result.append(QStringView(command).mid(last, match.capturedStart() - last));
        result.append(value);
        last = match.capturedEnd();
    }

    result.append(QStringView(command).mid(last));

    return result;
}

bool RunDialog::execute(const QString &command, ScintillaNext *editor)
{
    const QString trimmed = command.trimmed();

    // URLs are opened in the default browser
    if (trimmed.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) || trimmed.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        return QDesktopServices::openUrl(QUrl(expandVariables(trimmed, editor, true)));
    }

    const QString expanded = expandVariables(trimmed, editor);

    // A plain directory or file gets opened with the system default application
    QFileInfo info(expanded);
    if (info.exists() && (info.isDir() || !info.isExecutable())) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(expanded));
    }

    const QString workingDirectory = (editor && editor->isFile()) ? editor->getPath() : QDir::currentPath();

#ifdef Q_OS_WIN
    return QProcess::startDetached(QStringLiteral("cmd"), {QStringLiteral("/c"), expanded}, workingDirectory);
#else
    return QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), expanded}, workingDirectory);
#endif
}

void RunDialog::addToHistory(const QString &command)
{
    QStringList history = settings->value("Run/History").toStringList();

    history.removeAll(command);
    history.prepend(command);

    while (history.size() > 20) {
        history.removeLast();
    }

    settings->setValue("Run/History", history);
}

void RunDialog::run()
{
    const QString command = comboCommand->currentText().trimmed();

    if (command.isEmpty()) {
        return;
    }

    addToHistory(command);

    if (!execute(command, editor)) {
        QMessageBox::warning(this, tr("Run"), tr("Unable to run <b>%1</b>").arg(command.toHtmlEscaped()));
        return;
    }

    accept();
}

void RunDialog::save()
{
    const QString command = comboCommand->currentText().trimmed();

    if (command.isEmpty()) {
        return;
    }

    bool ok;
    const QString name = QInputDialog::getText(this, tr("Save Command"), tr("Name:"), QLineEdit::Normal, QString(), &ok);

    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    QList<QPair<QString, QString>> commands = savedCommands(settings);
    commands.append({name.trimmed(), command});
    setSavedCommands(settings, commands);

    emit savedCommandsChanged();
}

void RunDialog::browse()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select a program"));

    if (!file.isEmpty()) {
        QString quoted = QDir::toNativeSeparators(file);
        if (quoted.contains(QLatin1Char(' '))) {
            quoted = QStringLiteral("\"%1\"").arg(quoted);
        }

        comboCommand->setCurrentText(quoted);
    }
}
