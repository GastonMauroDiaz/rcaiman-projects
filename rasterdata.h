// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// This file is part of rcaiman-projects.
//
// rcaiman-projects is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// rcaiman-projects is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rcaiman-projects. If not, see <https://www.gnu.org/licenses/>.
//
// Part of the rcaiman suite.

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
    bool hasBandName(int band) const;
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
