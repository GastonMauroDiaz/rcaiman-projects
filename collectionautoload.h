#ifndef COLLECTIONAUTOLOAD_H
#define COLLECTIONAUTOLOAD_H

#include "layerstyle.h"
#include "project.h"

#include <QString>
#include <QVector>

class RasterData;

enum class AutoloadLayerKind {
    Image,
    Points
};

struct AutoloadCandidate
{
    Collection collection;
    QString filePath;
    AutoloadLayerKind layerKind = AutoloadLayerKind::Image;
    int priority = 0;
    bool openedFile = false;
};

class CollectionAutoloadResolver
{
public:
    QVector<AutoloadCandidate> resolve(
        const QString &projectRootPath,
        const QString &openedFilePath,
        const QVector<Collection> &collections) const;
};

class CollectionStyleResolver
{
public:
    ImageLayerStyle imageStyle(const Collection &collection,
                               const RasterData &rasterData) const;
    PointLayerStyle pointStyle(const Collection &collection,
                               int recordCount) const;
};

#endif // COLLECTIONAUTOLOAD_H
