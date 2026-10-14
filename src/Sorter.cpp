/*
 * This file is part of Notepad Next.
 * Copyright 2026 Justin Dailey
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

#include "Sorter.h"

#include <QCollator>
#include <QRandomGenerator>
#include <QString>

#include <algorithm>
#include <optional>

void CaseInsensitiveSorter::sort(QVector<QByteArrayView> &lines) const
{
    std::stable_sort(lines.begin(), lines.end(),
        [dir = direction](const QByteArrayView &a, const QByteArrayView &b) {
            int result = a.compare(b, Qt::CaseInsensitive);
            return dir == Direction::Ascending ? (result < 0) : (result > 0);
        });
}

void CaseSensitiveSorter::sort(QVector<QByteArrayView> &lines) const
{
    std::stable_sort(lines.begin(), lines.end(),
        [dir = direction](const QByteArrayView &a, const QByteArrayView &b) {
            int result = a.compare(b, Qt::CaseSensitive);
            return dir == Direction::Ascending ? (result < 0) : (result > 0);
        });
}

void LineLengthSorter::sort(QVector<QByteArrayView> &lines) const
{
    std::stable_sort(lines.begin(), lines.end(),
        [dir = direction](const QByteArrayView &a, const QByteArrayView &b) {
            return dir == Direction::Ascending ? (a.size() < b.size()) : (a.size() > b.size());
        });
}

void ReverseSorter::sort(QVector<QByteArrayView> &lines) const
{
    std::reverse(lines.begin(), lines.end());
}


template<typename T, typename Parser>
static void sortNumerically(QVector<QByteArrayView> &lines, Sorter::Direction direction, Parser parser)
{
    QVector<QPair<T, QByteArrayView>> numbers;
    QVector<QByteArrayView> others;

    for (const QByteArrayView &line : lines) {
        std::optional<T> value = parser(line);

        if (value.has_value()) {
            numbers.append({value.value(), line});
        }
        else {
            others.append(line);
        }
    }

    std::stable_sort(numbers.begin(), numbers.end(), [direction](const QPair<T, QByteArrayView> &a, const QPair<T, QByteArrayView> &b) {
        return direction == Sorter::Direction::Ascending ? (a.first < b.first) : (a.first > b.first);
    });

    lines.clear();
    for (const auto &pair : numbers) {
        lines.append(pair.second);
    }
    lines.append(others);
}

void IntegerSorter::sort(QVector<QByteArrayView> &lines) const
{
    sortNumerically<qlonglong>(lines, direction, [](const QByteArrayView &line) -> std::optional<qlonglong> {
        const QByteArray trimmed = line.toByteArray().trimmed();
        bool ok = false;
        const qlonglong value = trimmed.toLongLong(&ok);

        if (ok) return value;
        return std::nullopt;
    });
}

void DecimalSorter::sort(QVector<QByteArrayView> &lines) const
{
    const bool useComma = comma;

    sortNumerically<double>(lines, direction, [useComma](const QByteArrayView &line) -> std::optional<double> {
        QByteArray trimmed = line.toByteArray().trimmed();

        if (useComma) {
            // Thousand separators are dots, the decimal separator is a comma
            trimmed.replace('.', "");
            trimmed.replace(',', '.');
        }
        else {
            trimmed.replace(',', "");
        }

        bool ok = false;
        const double value = trimmed.toDouble(&ok);

        if (ok) return value;
        return std::nullopt;
    });
}

void LocaleSorter::sort(QVector<QByteArrayView> &lines) const
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    QVector<QPair<QString, QByteArrayView>> strings;
    strings.reserve(lines.size());

    for (const QByteArrayView &line : lines) {
        strings.append({QString::fromUtf8(line), line});
    }

    std::stable_sort(strings.begin(), strings.end(), [&](const QPair<QString, QByteArrayView> &a, const QPair<QString, QByteArrayView> &b) {
        const int result = collator.compare(a.first, b.first);
        return direction == Direction::Ascending ? (result < 0) : (result > 0);
    });

    for (int i = 0; i < strings.size(); ++i) {
        lines[i] = strings[i].second;
    }
}

void RandomSorter::sort(QVector<QByteArrayView> &lines) const
{
    std::shuffle(lines.begin(), lines.end(), *QRandomGenerator::global());
}
