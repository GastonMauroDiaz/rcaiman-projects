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
        value = index.row();
    } else if (index.column() == 1) {
        value = record.row;
    } else if (index.column() == 2) {
        value = record.col;
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
