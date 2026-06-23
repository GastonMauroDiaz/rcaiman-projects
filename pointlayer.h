#ifndef POINTLAYER_H
#define POINTLAYER_H

#include "layer.h"
#include "layerstyle.h"
#include "pointcollection.h"

class PointItem;
class QGraphicsScene;

class PointLayer : public Layer
{
    Q_OBJECT

public:
    explicit PointLayer(const QString &path, QGraphicsScene *scene, QObject *parent = nullptr);

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override;

    const PointCollection &collection() const { return m_collection; }
    PointLayerStyle style() const { return m_style; }
    void setStyle(const PointLayerStyle &style);
    void setColor(const QColor &color);
    void setSize(double size);
    void setSymbol(PointSymbol symbol);

    void setEditMode(bool enabled);
    bool editMode() const { return m_editMode; }
    void setActive(bool active) override;

    void cancelEdit();

    bool saveCsv(QString *errorMessage = nullptr);

    void addPoint(double row, double col);

    // Selection
    int selectedPointIndex() const { return m_selectedPointIndex; }
    const PointRecord *selectedPoint() const;
    void selectPoint(int index);
    void clearSelection();

signals:
    void selectedPointChanged(int index);  // -1 = deseleccionado

private slots:
    void onPointClicked(int index);
    void onPointMoved(int index, QPointF pos);
    void onPointDeleted(int index);

private:
    void rebuildItems(QGraphicsScene *scene);
    void applyStyleToItems();
    void applyStyleToPointItem(int index);
    QColor selectionColor() const;
    void clearPointItems();
    bool ensureBackup();
    void removeBackup();

    PointCollection m_collection;
    PointLayerStyle m_style;
    QGraphicsScene *m_scene = nullptr;
    bool m_editMode = false;
    QString m_backupPath;
    int m_selectedPointIndex = -1;
};

#endif // POINTLAYER_H
