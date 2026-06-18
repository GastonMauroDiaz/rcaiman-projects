#ifndef POINTCOLLECTION_H
#define POINTCOLLECTION_H

#include "overlay.h"

#include <QVector>
#include <QString>

class QGraphicsScene;

struct PointRecord {
    double row;
    double col;
};

class PointCollection : public Overlay
{
public:
    PointCollection() = default;

    static PointCollection load(const QString &filePath, QString *errorMessage = nullptr);
    static PointCollection loadFromDirectory(const QString &dirPath, QString *errorMessage = nullptr);

    // Overlay interface
    void addToScene(QGraphicsScene *scene) const override;

    bool isValid() const;
    const QVector<PointRecord> &points() const;

private:
    QVector<PointRecord> m_points;
};

#endif // POINTCOLLECTION_H