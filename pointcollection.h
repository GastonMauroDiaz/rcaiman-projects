#ifndef POINTCOLLECTION_H
#define POINTCOLLECTION_H

#include <QVector>
#include <QString>

struct PointRecord {
    double row;
    double col;
};

class PointCollection
{
public:
    PointCollection() = default;

    static PointCollection load(const QString &filePath, QString *errorMessage = nullptr);
    static PointCollection loadFromDirectory(const QString &dirPath, QString *errorMessage = nullptr);

    bool isValid() const;
    const QVector<PointRecord> &points() const;

private:
    QVector<PointRecord> m_points;
};

#endif // POINTCOLLECTION_H