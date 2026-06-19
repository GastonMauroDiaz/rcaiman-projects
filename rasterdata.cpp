#include "rasterdata.h"

#include <QVariant>

int RasterData::sampleSize() const
{
    switch (m_dataType) {
    case DataType::UInt8:   return 1;
    case DataType::UInt16:  return 2;
    case DataType::Int16:   return 2;
    case DataType::UInt32:  return 4;
    case DataType::Int32:   return 4;
    case DataType::Float32: return 4;
    case DataType::Float64: return 8;
    default:                return 0;
    }
}

void RasterData::clear()
{
    m_width = 0;
    m_height = 0;
    m_bands = 0;
    m_dataType = DataType::Invalid;
    m_bandNames.clear();
    m_data.clear();
}

QString RasterData::bandName(int band) const
{
    if (band >= 0 && band < m_bandNames.size() && !m_bandNames[band].isEmpty())
        return m_bandNames[band];
    return QStringLiteral("Band %1").arg(band);
}

void RasterData::setBandName(int band, const QString &name)
{
    if (band < 0)
        return;
    while (m_bandNames.size() <= band)
        m_bandNames.append(QString());
    m_bandNames[band] = name;
}

void RasterData::allocate(int width, int height, int bands, DataType type)
{
    clear();
    m_width = width;
    m_height = height;
    m_bands = bands;
    m_dataType = type;

    const qsizetype bytesPerBand = static_cast<qsizetype>(width) * height * sampleSize();
    m_data.resize(bytesPerBand * bands);
    m_data.fill(0);

    m_bandNames.reserve(bands);
    for (int i = 0; i < bands; ++i)
        m_bandNames.append(QString());
}

QVariant RasterData::sample(int band, int row, int col) const
{
    if (band < 0 || band >= m_bands || row < 0 || row >= m_height || col < 0 || col >= m_width || m_data.isEmpty())
        return {}; 

    const qsizetype offset = static_cast<qsizetype>(row) * m_width + col;
    switch (m_dataType) {
    case DataType::UInt8:
        return QVariant(static_cast<uint>(bandPointer<quint8>(band)[offset]));
    case DataType::UInt16:
        return QVariant(static_cast<uint>(bandPointer<quint16>(band)[offset]));
    case DataType::Int16:
        return QVariant(static_cast<int>(bandPointer<qint16>(band)[offset]));
    case DataType::UInt32:
        return QVariant(static_cast<uint>(bandPointer<quint32>(band)[offset]));
    case DataType::Int32:
        return QVariant(static_cast<int>(bandPointer<qint32>(band)[offset]));
    case DataType::Float32:
        return QVariant(bandPointer<float>(band)[offset]);
    case DataType::Float64:
        return QVariant(bandPointer<double>(band)[offset]);
    default:
        return {};
    }
}

qsizetype RasterData::bandByteOffset(int band) const
{
    if (band < 0 || band >= m_bands)
        return 0;
    return band * static_cast<qsizetype>(m_width) * m_height * sampleSize();
}
