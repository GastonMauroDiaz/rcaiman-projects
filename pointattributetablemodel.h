#ifndef POINTATTRIBUTETABLEMODEL_H
#define POINTATTRIBUTETABLEMODEL_H

#include <QAbstractTableModel>
#include <QPointer>

class PointLayer;

class PointAttributeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PointAttributeTableModel(PointLayer *layer,
                                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    PointLayer *layer() const { return m_layer; }
    void refresh();

private:
    QPointer<PointLayer> m_layer;
};

#endif // POINTATTRIBUTETABLEMODEL_H
