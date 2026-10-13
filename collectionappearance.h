// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#ifndef COLLECTIONAPPEARANCE_H
#define COLLECTIONAPPEARANCE_H

#include <QColor>
#include <QIcon>
#include <QStringList>

namespace CollectionAppearance {

QStringList classNames();
QColor inkColor(const QString &collectionClass);
QColor markerColor(const QString &collectionClass);
QColor surfaceColor(const QString &collectionClass);
QIcon markerIcon(const QString &collectionClass, int logicalSize = 14);
QIcon folderIcon(const QString &collectionClass, bool hasContent);
QIcon reservedFolderIcon(bool hasContent);

}

#endif // COLLECTIONAPPEARANCE_H
