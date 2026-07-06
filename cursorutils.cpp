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

#include "cursorutils.h"

#include <QPixmap>
#include <QSize>

QCursor highDpiCursor(const QString &resourcePath,
                      const QPoint &logicalHotspot,
                      Qt::CursorShape fallback)
{
    constexpr int logicalSize = 32;
    constexpr qreal sourceDevicePixelRatio = 2.0;
    const QSize sourceSize(
        qRound(logicalSize * sourceDevicePixelRatio),
        qRound(logicalSize * sourceDevicePixelRatio));

    QPixmap pixmap(resourcePath);
    if (pixmap.isNull())
        return QCursor(fallback);

    if (pixmap.size() != sourceSize) {
        pixmap = pixmap.scaled(
            sourceSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    }

    pixmap.setDevicePixelRatio(sourceDevicePixelRatio);
    return QCursor(pixmap, logicalHotspot.x(), logicalHotspot.y());
}
