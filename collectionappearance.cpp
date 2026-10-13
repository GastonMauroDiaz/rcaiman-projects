// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#include "collectionappearance.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

namespace {

struct ClassColors {
    const char *className;
    const char *ink;
    const char *marker;
    const char *surface;
};

constexpr ClassColors ClassPalette[] = {
    {"preprocessed_image_collection", "#2f6f3e", "#3da64a", "#d8ead3"},
    {"processed_image_collection", "#1f5f86", "#2b8fd3", "#d8e9f8"},
    {"point_collection", "#8a6200", "#d9a000", "#fff0c2"},
    {"segmentation_collection", "#6d3f8c", "#9b59b6", "#efe0f8"},
    {"other", "#9f2f2a", "#d94b45", "#f4c7c3"}
};

const ClassColors *colorsForClass(const QString &collectionClass)
{
    for (const ClassColors &colors : ClassPalette) {
        if (collectionClass == QLatin1String(colors.className))
            return &colors;
    }
    return nullptr;
}

QPixmap folderPixmap(int size,
                     const QColor &ink,
                     const QColor &marker,
                     bool hasContent)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size / 16.0, size / 16.0);
    painter.setPen(Qt::NoPen);

    QPainterPath back;
    back.moveTo(1.2, 4.8);
    back.lineTo(1.2, 3.0);
    back.quadTo(1.2, 2.1, 2.1, 2.1);
    back.lineTo(6.4, 2.1);
    back.quadTo(6.9, 2.1, 7.2, 2.6);
    back.lineTo(8.5, 4.2);
    back.lineTo(13.9, 4.2);
    back.quadTo(14.8, 4.2, 14.8, 5.1);
    back.lineTo(14.8, 12.7);
    back.lineTo(1.2, 12.7);
    back.closeSubpath();
    painter.fillPath(back, ink);

    if (hasContent) {
        QPainterPath paper;
        paper.moveTo(4.0, 4.5);
        paper.lineTo(13.8, 4.5);
        paper.lineTo(13.8, 10.7);
        paper.lineTo(2.8, 10.7);
        paper.lineTo(2.8, 5.7);
        paper.closeSubpath();
        painter.fillPath(paper, Qt::white);
    }

    QPainterPath front;
    front.moveTo(1.2, 7.0);
    front.quadTo(1.2, 6.3, 1.9, 6.3);
    front.lineTo(14.1, 6.3);
    front.quadTo(14.8, 6.3, 14.8, 7.0);
    front.lineTo(14.8, 13.1);
    front.quadTo(14.8, 14.0, 13.9, 14.0);
    front.lineTo(2.1, 14.0);
    front.quadTo(1.2, 14.0, 1.2, 13.1);
    front.closeSubpath();
    painter.fillPath(front, marker);

    return pixmap;
}

QIcon folderIconForColors(const QString &cacheKey,
                          const QColor &ink,
                          const QColor &marker,
                          bool hasContent)
{
    if (!ink.isValid() || !marker.isValid())
        return {};

    static QHash<QString, QIcon> cache;
    const QString stateKey = QStringLiteral("%1:%2")
        .arg(cacheKey)
        .arg(hasContent ? 1 : 0);
    const auto cached = cache.constFind(stateKey);
    if (cached != cache.cend())
        return cached.value();

    QIcon icon;
    const QList<int> sizes = {16, 20, 24, 32, 48, 64};
    for (const int size : sizes)
        icon.addPixmap(folderPixmap(size, ink, marker, hasContent));

    cache.insert(stateKey, icon);
    return icon;
}

}

QStringList CollectionAppearance::classNames()
{
    QStringList names;
    for (const ClassColors &colors : ClassPalette)
        names.append(QLatin1String(colors.className));
    return names;
}

QColor CollectionAppearance::markerColor(const QString &collectionClass)
{
    const ClassColors *colors = colorsForClass(collectionClass);
    return colors ? QColor(QLatin1String(colors->marker)) : QColor();
}

QColor CollectionAppearance::inkColor(const QString &collectionClass)
{
    const ClassColors *colors = colorsForClass(collectionClass);
    return colors ? QColor(QLatin1String(colors->ink)) : QColor();
}

QColor CollectionAppearance::surfaceColor(const QString &collectionClass)
{
    const ClassColors *colors = colorsForClass(collectionClass);
    return colors ? QColor(QLatin1String(colors->surface)) : QColor("#e6e6e6");
}

QIcon CollectionAppearance::markerIcon(const QString &collectionClass,
                                       int logicalSize)
{
    const QColor color = markerColor(collectionClass);
    const QColor outline = inkColor(collectionClass);
    if (!color.isValid() || logicalSize <= 0)
        return {};

    QPixmap pixmap(logicalSize, logicalSize);
    pixmap.fill(Qt::transparent);

    const qreal diameter = qMax(6.0, logicalSize - 6.0);
    const qreal offset = (logicalSize - diameter) / 2.0;

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(outline, 0.8));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(offset, offset, diameter, diameter));

    return QIcon(pixmap);
}

QIcon CollectionAppearance::folderIcon(const QString &collectionClass,
                                       bool hasContent)
{
    const QColor ink = inkColor(collectionClass);
    const QColor marker = markerColor(collectionClass);
    return folderIconForColors(
        collectionClass, ink, marker, hasContent);
}

QIcon CollectionAppearance::reservedFolderIcon(bool hasContent)
{
    return folderIconForColors(
        QStringLiteral("reserved"),
        QColor("#606873"),
        QColor("#9aa3ad"),
        hasContent);
}
