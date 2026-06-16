#ifndef TIFFREADER_H
#define TIFFREADER_H

#include "rasterdata.h"
#include <QString>

class TiffReader
{
public:
    static RasterData load(const QString &fileName, QString *errorMessage = nullptr);
};

#endif // TIFFREADER_H