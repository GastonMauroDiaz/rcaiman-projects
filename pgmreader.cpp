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

#include "pgmreader.h"

#include <QFile>

#include <cctype>
#include <cstring>

namespace {
bool readToken(QFile *file, QByteArray *token)
{
    if (!file || !token)
        return false;

    token->clear();
    char c = '\0';
    while (file->getChar(&c)) {
        if (std::isspace(static_cast<unsigned char>(c)))
            continue;
        if (c == '#') {
            file->readLine();
            continue;
        }
        token->append(c);
        break;
    }

    if (token->isEmpty())
        return false;

    while (file->getChar(&c)) {
        if (std::isspace(static_cast<unsigned char>(c)))
            break;
        if (c == '#') {
            file->readLine();
            break;
        }
        token->append(c);
    }
    return true;
}
}

RasterData PgmReader::load(const QString &fileName, QString *errorMessage)
{
    RasterData result;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Could not open PGM file: %1")
                .arg(fileName);
        return result;
    }

    QByteArray token;
    if (!readToken(&file, &token) || token != "P5") {
        if (errorMessage)
            *errorMessage = QStringLiteral("Only binary PGM P5 files are supported.");
        return result;
    }

    bool ok = false;
    if (!readToken(&file, &token)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing PGM width.");
        return result;
    }
    const int width = token.toInt(&ok);
    if (!ok || width <= 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid PGM width.");
        return result;
    }

    if (!readToken(&file, &token)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing PGM height.");
        return result;
    }
    const int height = token.toInt(&ok);
    if (!ok || height <= 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid PGM height.");
        return result;
    }

    if (!readToken(&file, &token)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing PGM maximum value.");
        return result;
    }
    const int maxValue = token.toInt(&ok);
    if (!ok || maxValue <= 0 || maxValue > 65535) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Invalid PGM maximum value.");
        return result;
    }

    const bool sixteenBit = maxValue > 255;
    const int bytesPerSample = sixteenBit ? 2 : 1;
    const qsizetype pixelCount =
        static_cast<qsizetype>(width) * static_cast<qsizetype>(height);
    const qsizetype expectedBytes = pixelCount * bytesPerSample;
    const QByteArray pixels = file.read(expectedBytes);
    if (pixels.size() != expectedBytes) {
        if (errorMessage)
            *errorMessage = QStringLiteral("PGM pixel data is truncated.");
        return result;
    }

    result.allocate(width, height, 1,
                    sixteenBit ? RasterData::DataType::UInt16
                               : RasterData::DataType::UInt8);
    if (!sixteenBit) {
        std::memcpy(result.rawData().data(), pixels.constData(),
                    static_cast<size_t>(expectedBytes));
        return result;
    }

    auto *dst = reinterpret_cast<quint16 *>(result.rawData().data());
    const auto *src =
        reinterpret_cast<const unsigned char *>(pixels.constData());
    for (qsizetype i = 0; i < pixelCount; ++i) {
        dst[i] = static_cast<quint16>(
            (static_cast<quint16>(src[i * 2]) << 8)
            | static_cast<quint16>(src[i * 2 + 1]));
    }
    return result;
}
