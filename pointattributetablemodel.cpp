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

#include "pointattributetablemodel.h"
#include "pointlayer.h"

PointAttributeTableModel::PointAttributeTableModel(
    PointLayer *layer,
    QObject *parent)
    : QAbstractTableModel(parent)
    , m_layer(layer)
{
}

int PointAttributeTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_layer)
        return 0;
    return m_layer->collection().recordCount();
}

int PointAttributeTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_layer)
        return 0;
    return 1 + m_layer->collection().columnNames().size();
}

QVariant PointAttributeTableModel::data(const QModelIndex &index,
                                        int role) const
{
    if (!m_layer || !index.isValid()
        || index.row() < 0
        || index.row() >= m_layer->collection().recordCount())
        return {};

    const PointRecord &record =
        m_layer->collection().points().at(index.row());
    QVariant value;
    if (index.column() == 0) {
        value = index.row() + 1;
    } else if (index.column() == 1) {
        value = record.x;
    } else if (index.column() == 2) {
        value = record.yDown;
    } else {
        const int attributeIndex = index.column() - 3;
        if (attributeIndex >= 0
            && attributeIndex < record.attributes.size())
            value = record.attributes.at(attributeIndex);
    }

    if (role == Qt::DisplayRole)
        return value.isValid() ? value : QVariant(QStringLiteral("NA"));
    if (role == Qt::UserRole)
        return value;
    if (role == Qt::TextAlignmentRole
        && (index.column() <= 2
            || (value.isValid()
                && value.metaType().id() != QMetaType::QString))) {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }
    return {};
}

QVariant PointAttributeTableModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return section + 1;
    if (!m_layer)
        return {};
    if (section == 0)
        return QStringLiteral("ID");

    const QStringList columns = m_layer->collection().columnNames();
    const int columnIndex = section - 1;
    return columnIndex >= 0 && columnIndex < columns.size()
        ? QVariant(columns.at(columnIndex)) : QVariant();
}

Qt::ItemFlags PointAttributeTableModel::flags(
    const QModelIndex &index) const
{
    return index.isValid()
        ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
        : Qt::NoItemFlags;
}

void PointAttributeTableModel::refresh()
{
    beginResetModel();
    endResetModel();
}
