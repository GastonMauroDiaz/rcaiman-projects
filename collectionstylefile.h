#ifndef COLLECTIONSTYLEFILE_H
#define COLLECTIONSTYLEFILE_H

#include "layerstyle.h"

#include <QString>

struct CollectionStyleData
{
    enum class Kind {
        None,
        Point,
        Image
    };

    Kind kind = Kind::None;
    PointLayerStyle pointStyle;
    ImageLayerStyle imageStyle;
};

class CollectionStyleFile
{
public:
    static QString filePath(const QString &collectionDirectory);
    static CollectionStyleData load(const QString &collectionDirectory);
    static bool save(const QString &collectionDirectory,
                     const CollectionStyleData &data,
                     QString *errorMessage = nullptr);
    static bool remove(const QString &collectionDirectory,
                       QString *errorMessage = nullptr);
};

#endif // COLLECTIONSTYLEFILE_H
