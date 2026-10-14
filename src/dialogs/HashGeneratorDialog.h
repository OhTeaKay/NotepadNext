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

#include <QCryptographicHash>
#include <QDialog>

class QCheckBox;
class QPlainTextEdit;
class QPushButton;

// Notepad++'s "Tools > MD5/SHA-1/SHA-256/SHA-512" generators
class HashGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Text,
        Files
    };

    HashGeneratorDialog(QCryptographicHash::Algorithm algorithm, Mode mode, QWidget *parent = nullptr);

    static QString algorithmName(QCryptographicHash::Algorithm algorithm);
    static QString hash(const QByteArray &data, QCryptographicHash::Algorithm algorithm);
    static QString hashFile(const QString &filePath, QCryptographicHash::Algorithm algorithm);

private slots:
    void updateTextHash();
    void chooseFiles();

private:
    QCryptographicHash::Algorithm algorithm;

    QPlainTextEdit *input = nullptr;
    QCheckBox *checkEachLine = nullptr;
    QPlainTextEdit *output;
};
