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

#include "HashGeneratorDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>


HashGeneratorDialog::HashGeneratorDialog(QCryptographicHash::Algorithm algorithm, Mode mode, QWidget *parent) :
    QDialog(parent, Qt::Dialog),
    algorithm(algorithm)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(700, 400);

    const QString name = algorithmName(algorithm);
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    QVBoxLayout *layout = new QVBoxLayout(this);

    output = new QPlainTextEdit();
    output->setReadOnly(true);
    output->setFont(fixedFont);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);

    QPushButton *buttonCopy = new QPushButton(tr("Copy to Clipboard"));
    QPushButton *buttonClose = new QPushButton(tr("Close"));
    connect(buttonCopy, &QPushButton::clicked, this, [=]() { QApplication::clipboard()->setText(output->toPlainText()); });
    connect(buttonClose, &QPushButton::clicked, this, &QDialog::close);

    if (mode == Mode::Text) {
        setWindowTitle(tr("Generate %1 digest").arg(name));

        input = new QPlainTextEdit();
        input->setFont(fixedFont);
        checkEachLine = new QCheckBox(tr("Treat each line as a separate string"));

        connect(input, &QPlainTextEdit::textChanged, this, &HashGeneratorDialog::updateTextHash);
        connect(checkEachLine, &QCheckBox::toggled, this, &HashGeneratorDialog::updateTextHash);

        layout->addWidget(new QLabel(tr("Text:")));
        layout->addWidget(input, 1);
        layout->addWidget(checkEachLine);
        layout->addWidget(new QLabel(tr("%1 digest:").arg(name)));
        layout->addWidget(output, 1);

        input->setFocus();
    }
    else {
        setWindowTitle(tr("Generate %1 digest from files").arg(name));

        QPushButton *buttonChoose = new QPushButton(tr("Choose files to generate %1...").arg(name));
        connect(buttonChoose, &QPushButton::clicked, this, &HashGeneratorDialog::chooseFiles);

        layout->addWidget(buttonChoose);
        layout->addWidget(output, 1);
    }

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(buttonCopy);
    buttons->addWidget(buttonClose);
    layout->addLayout(buttons);
}

QString HashGeneratorDialog::algorithmName(QCryptographicHash::Algorithm algorithm)
{
    switch (algorithm) {
    case QCryptographicHash::Md5: return QStringLiteral("MD5");
    case QCryptographicHash::Sha1: return QStringLiteral("SHA-1");
    case QCryptographicHash::Sha256: return QStringLiteral("SHA-256");
    case QCryptographicHash::Sha512: return QStringLiteral("SHA-512");
    default: return QStringLiteral("Hash");
    }
}

QString HashGeneratorDialog::hash(const QByteArray &data, QCryptographicHash::Algorithm algorithm)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, algorithm).toHex());
}

QString HashGeneratorDialog::hashFile(const QString &filePath, QCryptographicHash::Algorithm algorithm)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hasher(algorithm);
    hasher.addData(&file);

    return QString::fromLatin1(hasher.result().toHex());
}

void HashGeneratorDialog::updateTextHash()
{
    const QString text = input->toPlainText();

    if (text.isEmpty()) {
        output->clear();
        return;
    }

    if (checkEachLine->isChecked()) {
        QStringList hashes;

        for (const QString &line : text.split(QLatin1Char('\n'))) {
            hashes.append(line.isEmpty() ? QString() : hash(line.toUtf8(), algorithm));
        }

        output->setPlainText(hashes.join(QLatin1Char('\n')));
    }
    else {
        output->setPlainText(hash(text.toUtf8(), algorithm));
    }
}

void HashGeneratorDialog::chooseFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Choose files to generate %1").arg(algorithmName(algorithm)));

    if (files.isEmpty()) {
        return;
    }

    QStringList lines;
    for (const QString &file : files) {
        lines.append(QStringLiteral("%1  %2").arg(hashFile(file, algorithm), QDir::toNativeSeparators(file)));
    }

    output->setPlainText(lines.join(QLatin1Char('\n')));
}
