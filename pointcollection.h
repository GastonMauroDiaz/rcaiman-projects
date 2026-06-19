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