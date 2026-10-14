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

#pragma once

#include <QList>
#include <QByteArray>

class Sorter
{
public:
    enum class Direction {
        Ascending,
        Descending
    };

    explicit Sorter(Direction direction) : direction(direction) {}
    virtual ~Sorter() = default;

    virtual void sort(QVector<QByteArrayView> &lines) const = 0;

protected:
    Direction direction;
};


class CaseInsensitiveSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};


class CaseSensitiveSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};


class LineLengthSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};


class ReverseSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};



// Sorts lines by their numeric value. Lines that are not numbers are kept at the end in their original order.
class IntegerSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};


class DecimalSorter : public Sorter
{
public:
    DecimalSorter(Direction direction, bool commaIsDecimalSeparator) : Sorter(direction), comma(commaIsDecimalSeparator) {}
    void sort(QVector<QByteArrayView> &lines) const override;

private:
    bool comma;
};


// Sorts lines according to the rules of the current locale (e.g. accented letters are sorted next to their base letter)
class LocaleSorter : public Sorter
{
public:
    using Sorter::Sorter;
    void sort(QVector<QByteArrayView> &lines) const override;
};


class RandomSorter : public Sorter
{
public:
    RandomSorter() : Sorter(Direction::Ascending) {}
    void sort(QVector<QByteArrayView> &lines) const override;
};
