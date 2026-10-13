// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#ifndef COLLECTIONCONTRACT_H
#define COLLECTIONCONTRACT_H

#include "project.h"

#include <QHash>

enum class CollectionContractSeverity {
    Error,
    Warning
};

struct CollectionContractIssue {
    CollectionContractSeverity severity = CollectionContractSeverity::Error;
    QString collectionId;
    QString collectionLabel;
    QString message;
};

struct CollectionGraphPlacement {
    QString frameId;
    QString branchRootId;
    int preprocessingLevel = -1;
    bool resolved = false;
};

struct CollectionContractAnalysis {
    int checkedCollections = 0;
    QVector<CollectionContractIssue> issues;
    QHash<QString, CollectionGraphPlacement> placements;

    int errorCount() const;
    int warningCount() const;
    bool hasIssues() const;
    bool hasIssuesForCollection(const QString &collectionId) const;
};

namespace CollectionContract {

constexpr int SchemaVersion = 2;

QString collectionId(const QString &path);
QString collectionLabel(const Collection &collection);
CollectionContractAnalysis analyze(
    const QString &projectRoot,
    const QVector<Collection> &collections);

}

#endif // COLLECTIONCONTRACT_H
