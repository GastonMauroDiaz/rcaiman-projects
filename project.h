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

#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QStringList>
#include <QVector>

struct Collection {
    QString dirPath;        // absolute path to the folder
    QString dirName;        // folder name (basename of dirPath)
    QString collectionClass;
    QString parent;
    QString related;
    QString registry;
    QString script;
};

enum class ProjectCleanupCategory {
    RawFiles,
    SensorImages,
    CollectionImages,
    Segmentations,
    PointFiles,
    OtherCollections,
    RegistryArtifacts,
    RootFiles,
    TemporaryBackups
};

struct ProjectCleanupGroup {
    ProjectCleanupCategory category;
    QString label;
    QStringList relativeFiles;
    bool selected = true;
};

struct ProjectCleanupPlan {
    QVector<ProjectCleanupGroup> groups;

    int totalSelectedFiles() const;
};

class Project
{
public:
    Project() = default;

    static Project create(const QString &rootPath,
                          const QString &projectName,
                          const QString &description = {},
                          QString *errorMessage = nullptr);
    static Project open(const QString &rootPath, QString *errorMessage = nullptr);

    // Register a collection: creates collection.yaml inside dirPath.
    static bool registerCollection(const QString &projectRootPath,
                                   const QString &dirPath,
                                   const QString &collectionClass,
                                   const QString &parent,
                                   const QString &related,
                                   const QString &registry,
                                   const QString &script,
                                   QString *errorMessage = nullptr);

    // Discover collections: scan first-level subdirectories of rootPath
    // looking for collection.yaml files.
    static QVector<Collection> discoverCollections(const QString &rootPath);

    static QString resolveReference(const QString &projectRootPath,
                                    const QString &reference);
    static QString portableReference(const QString &projectRootPath,
                                     const QString &reference);
    static bool descriptionIsSupported(const QString &description,
                                       QString *errorMessage = nullptr);

    bool isValid() const;
    bool save(QString *errorMessage = nullptr) const;
    bool setDescription(const QString &description,
                        QString *errorMessage = nullptr);
    bool writeReport(QString *errorMessage = nullptr) const;
    Project saveAs(const QString &newRootPath,
                   const QString &newProjectName,
                   QString *errorMessage = nullptr) const;
    static Project copyExistingProject(const QString &sourceRootPath,
                                       const QString &destinationRootPath,
                                       const QString &newProjectName,
                                       QString *errorMessage = nullptr);
    ProjectCleanupPlan buildCleanupPlan() const;
    bool applyCleanupPlan(const ProjectCleanupPlan &plan,
                          QString *errorMessage = nullptr) const;

    QString rootPath() const;
    QString name() const;
    QString created() const;
    QString description() const;
    QString reportFilePath() const;

    static QStringList reservedDirectories();

    static QStringList projectSubdirectories();

private:
    Project(QString rootPath, QString name, QString created,
            QString description = {});

    static QString metadataFilePath(const QString &rootPath);
    static bool createDirectoryStructure(const QString &rootPath, QString *errorMessage);

    QString m_rootPath;
    QString m_name;
    QString m_created;
    QString m_description;
};

#endif // PROJECT_H
