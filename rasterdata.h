#ifndef RASTERDATA_H
#define RASTERDATA_H

#include <QByteArray>
#include <QStringList>

class RasterData
{
public:
    enum class DataType : quint8 {
        UInt8,
        UInt16,
        Int16,
        UInt32,
        Int32,
        Float32,
        Float64,
        Invalid
    };

    RasterData() = default;

    // Metadata
    int width() const { return m_width; }
    int height() const { return m_height; }
    int bands() const { return m_bands; }
    DataType dataType() const { return m_dataType; }
    int sampleSize() const;  // bytes per sample
    bool isNull() const { return m_data.isEmpty(); }
    void clear();

    // Band names
    QString bandName(int band) const;
    void setBandName(int band, const QString &name);

    // Typed direct pointer to a band (planar storage).
    // The caller must know the DataType and use the corresponding T.
    template<typename T>
    const T *bandPointer(int band) const;

    // Raw byte access (for internal use by readers).
    const QByteArray &rawData() const { return m_data; }
    QByteArray &rawData() { return m_data; }

    // Construct a valid RasterData with planar storage.
    // Allocates width * height * sampleSize bytes per band.
    void allocate(int width, int height, int bands, DataType type);

    QVariant sample(int band, int row, int col) const;

private:
    qsizetype bandByteOffset(int band) const;

    int m_width = 0;
    int m_height = 0;
    int m_bands = 0;
    DataType m_dataType = DataType::Invalid;
    QStringList m_bandNames;
    QByteArray m_data;  // planar: [band0][band1]...
};

// Template implementation (must be in header for implicit instantiation).
template<typename T>
const T *RasterData::bandPointer(int band) const
{
    if (band < 0 || band >= m_bands || m_data.isEmpty())
        return nullptr;

    const qsizetype offset = band * static_cast<qsizetype>(m_width) * m_height;
    return reinterpret_cast<const T *>(m_data.constData()) + offset;
}

#endif // RASTERDATA_H