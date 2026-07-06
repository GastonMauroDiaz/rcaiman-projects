// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

#ifndef POINTCOLLECTION_H
#define POINTCOLLECTION_H

#include <QVector>
#include <QString>
#include <QVariant>
#include <QMetaType>

class QGraphicsScene;

struct ColumnMeta {
    QString name;
    QMetaType::Type type = QMetaType::Double;
};

struct PointRecord {
    double row;
    double col;
    QVector<QVariant> attributes;  // columnas extra (después de row,col)
};

class PointCollection
{
public:
    PointCollection() = default;

    static PointCollection load(const QString &filePath, QString *errorMessage = nullptr);
    static PointCollection loadFromDirectory(const QString &dirPath, QString *errorMessage = nullptr);
    static PointCollection createEmpty(const QStringList &columnNames = {"row", "col"});

    bool isValid() const;
    const QVector<PointRecord> &points() const;

    void addPoint(double row, double col);
    void removePoint(int index);
    void movePoint(int index, double row, double col);
    bool save(const QString &filePath, QString *errorMessage = nullptr) const;

    QStringList columnNames() const;
    const QVector<ColumnMeta> &columnMeta() const { return m_columnMeta; }
    int recordCount() const { return m_points.size(); }

private:
    QVector<PointRecord> m_points;
    QVector<ColumnMeta> m_columnMeta;
};

#endif // POINTCOLLECTION_H