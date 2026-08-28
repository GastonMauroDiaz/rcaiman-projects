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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "apppreferences.h"
#include "imageviewer.h"
#include "layersdock.h"
#include "layermanager.h"
#include "layer.h"
#include "imagelayer.h"
#include "pointlayer.h"
#include "pointcollection.h"
#include "pointcsvcreationdialog.h"
#include "pointgridcreationdialog.h"
#include "pointattributetablewindow.h"
#include "histogramdock.h"
#include "previewdock.h"
#include "projectcleandialog.h"
#include "projectcleanupdialog.h"
#include "projectfilesystemmodel.h"
#include "rawreader.h"
#include "collectiondialog.h"
#include "collectioneditdialog.h"
#include "collectiongraphdialog.h"
#include "collectionautoload.h"
#include "collectionstyledialog.h"
#include "newprojectdialog.h"
#include "metadataregistry.h"
#include "metadatarulesdialog.h"
#include "registryvalidationdialog.h"
#include "collectionvalidation.h"
#include "collectionvalidationdialog.h"
#include "acquisitionmetadata.h"
#include "tiffreader.h"
#include "tiffwriter.h"
#include "rasterrenderer.h"
#include "exampleprojectsdialog.h"
#include "legacyjpegimportdialog.h"
#include "legacyjpegimporter.h"
#include "hspimportdialog.h"
#include "hspprojectimporter.h"
#include "shortcuttext.h"

#include <QAction>
#include <QAbstractProxyModel>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDirIterator>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSizePolicy>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QSet>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
enum class ProjectExplorerSortMode {
    Name = 0,
    Type = 1
};

class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(Qt::TextElideMode mode, QWidget *parent = nullptr)
        : QLabel(parent)
        , m_mode(mode)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setFullText(const QString &text)
    {
        m_fullText = text;
        setToolTip(text);
        updateElidedText();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateElidedText();
    }

private:
    void updateElidedText()
    {
        setText(fontMetrics().elidedText(m_fullText, m_mode, width()));
    }

    QString m_fullText;
    Qt::TextElideMode m_mode;
};

struct MaskCandidate {
    Collection collection;
    QString path;
    QString label;
};

QString normalizedProjectReference(const QString &projectRoot,
                                   const QString &reference)
{
    if (projectRoot.isEmpty() || reference.trimmed().isEmpty())
        return {};
    QString resolved = Project::resolveReference(projectRoot, reference);
    if (resolved.isEmpty())
        return {};
    resolved = QDir::cleanPath(QFileInfo(resolved).absoluteFilePath());
#ifdef Q_OS_WIN
    resolved = resolved.toCaseFolded();
#endif
    return resolved;
}

QString normalizedAbsolutePathForComparison(const QString &path)
{
    if (path.trimmed().isEmpty())
        return {};
    QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

bool normalizedPathIsInside(const QString &path,
                            const QString &directoryPath)
{
    const QString normalizedPath =
        normalizedAbsolutePathForComparison(path);
    const QString normalizedDirectory =
        normalizedAbsolutePathForComparison(directoryPath);
    if (normalizedPath.isEmpty() || normalizedDirectory.isEmpty())
        return false;
    return normalizedPath == normalizedDirectory
        || normalizedPath.startsWith(normalizedDirectory + QLatin1Char('/'));
}

bool referenceEqualsDirectory(const QString &projectRoot,
                              const QString &reference,
                              const QString &directoryPath)
{
    return normalizedProjectReference(projectRoot, reference)
        == normalizedAbsolutePathForComparison(directoryPath);
}

bool referenceIsInsideDirectory(const QString &projectRoot,
                                const QString &reference,
                                const QString &directoryPath)
{
    const QString resolved =
        Project::resolveReference(projectRoot, reference);
    return normalizedPathIsInside(resolved, directoryPath);
}

QString collectionImpactLabel(const Collection &collection)
{
    QString label = collection.dirName;
    if (!collection.alias.isEmpty())
        label += QStringLiteral(" | %1").arg(collection.alias);
    return QStringLiteral("- %1 (%2)")
        .arg(label, collection.collectionClass);
}

QStringList collectionImpactLabels(const QVector<Collection> &collections)
{
    QStringList labels;
    labels.reserve(collections.size());
    for (const Collection &collection : collections)
        labels.append(collectionImpactLabel(collection));
    labels.sort(Qt::CaseInsensitive);
    return labels;
}

void appendImpactSection(QStringList *lines,
                         const QString &title,
                         const QStringList &items)
{
    if (!lines || items.isEmpty())
        return;
    lines->append(title);
    lines->append(items);
    lines->append(QString());
}

QStringList scriptFilesMentioningCollection(
    const QString &projectRoot,
    const QString &collectionDirPath)
{
    const QDir root(projectRoot);
    const QString scriptsPath = root.filePath("scripts");
    if (!QDir(scriptsPath).exists())
        return {};

    QStringList needles;
    const QString relativePath =
        QDir::fromNativeSeparators(root.relativeFilePath(collectionDirPath));
    const QString nativeRelativePath =
        QDir::toNativeSeparators(relativePath);
    const QString folderName = QFileInfo(collectionDirPath).fileName();
    for (const QString &needle : {
             relativePath,
             relativePath + QLatin1Char('/'),
             nativeRelativePath,
             nativeRelativePath + QLatin1Char('\\'),
             folderName}) {
        if (!needle.trimmed().isEmpty()
            && !needles.contains(needle, Qt::CaseInsensitive)) {
            needles.append(needle);
        }
    }

    QStringList matches;
    QDirIterator iterator(
        scriptsPath,
        {"*.R", "*.r", "*.Rmd", "*.rmd", "*.qmd"},
        QDir::Files | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString scriptPath = iterator.next();
        QFile file(scriptPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        const QString content = QString::fromUtf8(file.readAll());
        for (const QString &needle : std::as_const(needles)) {
            if (content.contains(needle, Qt::CaseInsensitive)) {
                matches.append(QStringLiteral("- %1").arg(
                    QDir::fromNativeSeparators(
                        root.relativeFilePath(scriptPath))));
                break;
            }
        }
    }
    matches.sort(Qt::CaseInsensitive);
    matches.removeDuplicates();
    return matches;
}

QString readPreviewTextFile(const QString &filePath, bool *truncated)
{
    if (truncated)
        *truncated = false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    constexpr qint64 PreviewTextByteLimit = 64 * 1024;
    const QByteArray bytes = file.read(PreviewTextByteLimit + 1);
    QByteArray previewBytes = bytes;
    if (previewBytes.size() > PreviewTextByteLimit) {
        previewBytes.truncate(PreviewTextByteLimit);
        if (truncated)
            *truncated = true;
    }

    QString text = QString::fromUtf8(previewBytes);
    if (truncated && *truncated) {
        text += QStringLiteral(
            "\n\n--- Preview truncated: showing the first 64 KB only. ---");
    }
    return text;
}

struct UnregisterImpact
{
    QVector<Collection> criticalParents;
    QVector<Collection> optionalParents;
    QVector<Collection> relatedLinks;
    QVector<Collection> registryFields;
    QVector<Collection> scriptFields;
    QStringList scriptMentions;

    bool hasCriticalParents() const
    {
        return !criticalParents.isEmpty();
    }

    bool hasCleanableNonCriticalReferences() const
    {
        return !optionalParents.isEmpty()
            || !relatedLinks.isEmpty()
            || !registryFields.isEmpty()
            || !scriptFields.isEmpty();
    }
};

struct UnregisterDecision
{
    bool accepted = false;
    bool cleanNonCriticalReferences = false;
    bool clearRequiredParents = true;
};

UnregisterImpact analyzeUnregisterImpact(
    const QString &projectRoot,
    const Collection &targetCollection,
    const QVector<Collection> &collections)
{
    UnregisterImpact impact;

    for (const Collection &collection : collections) {
        if (normalizedAbsolutePathForComparison(collection.dirPath)
            == normalizedAbsolutePathForComparison(targetCollection.dirPath)) {
            continue;
        }

        if (referenceEqualsDirectory(
                projectRoot, collection.parent, targetCollection.dirPath)) {
            if (collection.collectionClass == "point_collection"
                || collection.collectionClass == "segmentation_collection") {
                impact.criticalParents.append(collection);
            } else {
                impact.optionalParents.append(collection);
            }
        }
        if (referenceEqualsDirectory(
                projectRoot, collection.related, targetCollection.dirPath)) {
            impact.relatedLinks.append(collection);
        }
        if (referenceIsInsideDirectory(
                projectRoot, collection.registry, targetCollection.dirPath)) {
            impact.registryFields.append(collection);
        }
        if (referenceIsInsideDirectory(
                projectRoot, collection.script, targetCollection.dirPath)) {
            impact.scriptFields.append(collection);
        }
    }
    impact.scriptMentions = scriptFilesMentioningCollection(
        projectRoot, targetCollection.dirPath);
    return impact;
}

QString unregisterImpactReport(const UnregisterImpact &impact)
{
    QStringList lines;
    appendImpactSection(
        &lines,
        "Collections that use this folder as a required parent:",
        collectionImpactLabels(impact.criticalParents));
    appendImpactSection(
        &lines,
        "Collections that use this folder as an optional parent:",
        collectionImpactLabels(impact.optionalParents));
    appendImpactSection(
        &lines,
        "Collections that use this folder as related:",
        collectionImpactLabels(impact.relatedLinks));
    appendImpactSection(
        &lines,
        "Collections with registry files inside this folder:",
        collectionImpactLabels(impact.registryFields));
    appendImpactSection(
        &lines,
        "Collections with script files inside this folder:",
        collectionImpactLabels(impact.scriptFields));
    appendImpactSection(
        &lines,
        "Scripts that mention this folder name or path:",
        impact.scriptMentions);

    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    if (lines.isEmpty()) {
        return "No registered collections or scripts were found referencing "
               "this folder.";
    }
    return lines.join('\n');
}

UnregisterDecision confirmUnregisterCollection(
    QWidget *parent,
    const Collection &collection,
    const UnregisterImpact &impact)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Unregister collection");
    dialog.setMinimumWidth(620);

    auto *layout = new QVBoxLayout(&dialog);

    auto *intro = new QLabel(
        QStringLiteral(
            "Unregister collection \"%1\"?\n\n"
            "This turns the collection into a regular folder. "
            "Working files remain.")
            .arg(collection.dirName),
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *report = new QTextBrowser(&dialog);
    report->setPlainText(QStringLiteral("Impact report:\n%1").arg(
        unregisterImpactReport(impact)));
    report->setMinimumHeight(220);
    report->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(report);

    QCheckBox *cleanNonCritical = nullptr;
    if (impact.hasCleanableNonCriticalReferences()) {
        cleanNonCritical = new QCheckBox(
            "Clean non-required references in dependent collection metadata",
            &dialog);
        cleanNonCritical->setChecked(true);
        cleanNonCritical->setToolTip(
            "Clears optional parent, related, registry, and script fields "
            "that point to this folder. Scripts themselves are not edited.");
        layout->addWidget(cleanNonCritical);
    }

    QRadioButton *clearRequired = nullptr;
    QRadioButton *keepRequired = nullptr;
    if (impact.hasCriticalParents()) {
        auto *criticalGroup = new QGroupBox("Required parent links", &dialog);
        auto *criticalLayout = new QVBoxLayout(criticalGroup);
        auto *criticalText = new QLabel(
            "Some point or segmentation collections require a valid parent. "
            "After unregistering, they will need attention. Use Edit "
            "collection... to assign a new valid parent.",
            criticalGroup);
        criticalText->setWordWrap(true);
        criticalLayout->addWidget(criticalText);

        clearRequired = new QRadioButton(
            "Clear required parent fields",
            criticalGroup);
        clearRequired->setToolTip(
            "The dependent collections become explicitly orphaned until "
            "you assign a new parent.");
        keepRequired = new QRadioButton(
            "Keep existing parent paths",
            criticalGroup);
        keepRequired->setToolTip(
            "The dependent collections keep pointing to this folder, but it "
            "will no longer be a registered collection.");
        clearRequired->setChecked(true);
        criticalLayout->addWidget(clearRequired);
        criticalLayout->addWidget(keepRequired);
        layout->addWidget(criticalGroup);
    }

    auto *note = new QLabel(
        "Scripts are never edited automatically.",
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Unregister");
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setFocus();
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);

    UnregisterDecision decision;
    if (dialog.exec() != QDialog::Accepted)
        return decision;

    decision.accepted = true;
    decision.cleanNonCriticalReferences =
        cleanNonCritical && cleanNonCritical->isChecked();
    decision.clearRequiredParents =
        !clearRequired || clearRequired->isChecked();
    return decision;
}

void queueCollectionMetadataUpdate(QVector<Collection> *updates,
                                   const Collection &collection,
                                   bool clearParent,
                                   bool clearRelated,
                                   bool clearRegistry,
                                   bool clearScript)
{
    if (!updates)
        return;

    int index = -1;
    const QString targetPath =
        normalizedAbsolutePathForComparison(collection.dirPath);
    for (int i = 0; i < updates->size(); ++i) {
        if (normalizedAbsolutePathForComparison(updates->at(i).dirPath)
            == targetPath) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        updates->append(collection);
        index = updates->size() - 1;
    }

    Collection &updated = (*updates)[index];
    if (clearParent)
        updated.parent.clear();
    if (clearRelated)
        updated.related.clear();
    if (clearRegistry)
        updated.registry.clear();
    if (clearScript)
        updated.script.clear();
}

bool applyUnregisterDependencyCleanup(
    const QString &projectRoot,
    const UnregisterImpact &impact,
    const UnregisterDecision &decision,
    int *updatedCollections,
    QString *errorMessage)
{
    if (updatedCollections)
        *updatedCollections = 0;

    QVector<Collection> updates;
    if (decision.clearRequiredParents) {
        for (const Collection &collection : impact.criticalParents) {
            queueCollectionMetadataUpdate(
                &updates, collection, true, false, false, false);
        }
    }

    if (decision.cleanNonCriticalReferences) {
        for (const Collection &collection : impact.optionalParents) {
            queueCollectionMetadataUpdate(
                &updates, collection, true, false, false, false);
        }
        for (const Collection &collection : impact.relatedLinks) {
            queueCollectionMetadataUpdate(
                &updates, collection, false, true, false, false);
        }
        for (const Collection &collection : impact.registryFields) {
            queueCollectionMetadataUpdate(
                &updates, collection, false, false, true, false);
        }
        for (const Collection &collection : impact.scriptFields) {
            queueCollectionMetadataUpdate(
                &updates, collection, false, false, false, true);
        }
    }

    int written = 0;
    for (const Collection &collection : std::as_const(updates)) {
        if (!Project::updateCollectionMetadata(
                projectRoot,
                collection.dirPath,
                collection.alias,
                collection.parent,
                collection.related,
                collection.registry,
                collection.script,
                errorMessage)) {
            return false;
        }
        ++written;
    }

    if (updatedCollections)
        *updatedCollections = written;
    return true;
}

struct CollectionCheckIssue
{
    QString collectionLabel;
    QString message;
};

struct CollectionCheckResult
{
    int checkedCollections = 0;
    QVector<CollectionCheckIssue> errors;
    QVector<CollectionCheckIssue> warnings;
};

QString collectionDisplayLabel(const Collection &collection)
{
    QString label = collection.dirName;
    if (!collection.alias.isEmpty())
        label += QStringLiteral(" | %1").arg(collection.alias);
    if (collection.collectionClass.isEmpty())
        return label;
    return QStringLiteral("%1 (%2)").arg(label, collection.collectionClass);
}

int collectionIndexByPath(const QVector<Collection> &collections,
                          const QString &path)
{
    const QString normalizedPath = normalizedAbsolutePathForComparison(path);
    if (normalizedPath.isEmpty())
        return -1;

    for (int i = 0; i < collections.size(); ++i) {
        if (normalizedAbsolutePathForComparison(collections.at(i).dirPath)
            == normalizedPath) {
            return i;
        }
    }
    return -1;
}

int collectionIndexByDirectory(const QString &projectRoot,
                               const QVector<Collection> &collections,
                               const QString &directoryPath)
{
    const int byPath = collectionIndexByPath(collections, directoryPath);
    if (byPath >= 0)
        return byPath;

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir())
        return -1;

    // Collections are first-level project folders. The grouped explorer may
    // route through virtual rows, so keep a name-based fallback at that level.
    if (normalizedAbsolutePathForComparison(
            directoryInfo.absoluteDir().absolutePath())
        != normalizedAbsolutePathForComparison(projectRoot)) {
        return -1;
    }

    for (int i = 0; i < collections.size(); ++i) {
        if (collections.at(i).dirName.compare(
                directoryInfo.fileName(), Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void addCollectionCheckIssue(QVector<CollectionCheckIssue> *issues,
                             const Collection &collection,
                             const QString &message)
{
    if (!issues)
        return;
    issues->append({collectionDisplayLabel(collection), message});
}

bool collectionClassIsKnown(const QString &collectionClass)
{
    return QStringList{
        "preprocessed_image_collection",
        "processed_image_collection",
        "point_collection",
        "segmentation_collection",
        "other"
    }.contains(collectionClass, Qt::CaseInsensitive);
}

QStringList compatibleParentClassesForCheck(const QString &collectionClass)
{
    if (collectionClass == "processed_image_collection"
        || collectionClass == "point_collection"
        || collectionClass == "segmentation_collection") {
        return {"preprocessed_image_collection", "processed_image_collection"};
    }
    return {};
}

QStringList compatibleRelatedClassesForCheck(const QString &collectionClass)
{
    if (collectionClass == "point_collection")
        return {"segmentation_collection"};
    return {};
}

void checkCollectionReferenceToCollection(
    const QString &projectRoot,
    const QVector<Collection> &collections,
    const Collection &collection,
    const QString &fieldName,
    const QString &reference,
    const QStringList &allowedClasses,
    bool required,
    CollectionCheckResult *result)
{
    if (!result)
        return;

    if (reference.trimmed().isEmpty()) {
        if (required) {
            addCollectionCheckIssue(
                &result->errors,
                collection,
                QStringLiteral("%1 is required but empty.").arg(fieldName));
        }
        return;
    }

    const QString resolvedPath =
        Project::resolveReference(projectRoot, reference);
    const int referencedIndex =
        collectionIndexByPath(collections, resolvedPath);
    if (referencedIndex < 0) {
        const QFileInfo resolvedInfo(resolvedPath);
        const QString message = resolvedInfo.exists()
            ? QStringLiteral(
                  "%1 points to \"%2\", but that folder is not registered "
                  "as a collection.")
                  .arg(fieldName, QDir::toNativeSeparators(resolvedPath))
            : QStringLiteral(
                  "%1 points to missing folder \"%2\".")
                  .arg(fieldName, QDir::toNativeSeparators(resolvedPath));
        if (required) {
            addCollectionCheckIssue(&result->errors, collection, message);
        } else {
            addCollectionCheckIssue(&result->warnings, collection, message);
        }
        return;
    }

    const Collection &referenced = collections.at(referencedIndex);
    if (normalizedAbsolutePathForComparison(referenced.dirPath)
        == normalizedAbsolutePathForComparison(collection.dirPath)) {
        addCollectionCheckIssue(
            required ? &result->errors : &result->warnings,
            collection,
            QStringLiteral("%1 points to the collection itself.").arg(
                fieldName));
        return;
    }

    if (!allowedClasses.contains(
            referenced.collectionClass, Qt::CaseInsensitive)) {
        const QString message = QStringLiteral(
            "%1 points to %2, but expected %3.")
            .arg(fieldName,
                 collectionDisplayLabel(referenced),
                 allowedClasses.join(", "));
        if (required) {
            addCollectionCheckIssue(&result->errors, collection, message);
        } else {
            addCollectionCheckIssue(&result->warnings, collection, message);
        }
    }
}

void checkCollectionReferenceToFile(
    const QString &projectRoot,
    const Collection &collection,
    const QString &fieldName,
    const QString &reference,
    CollectionCheckResult *result)
{
    if (!result || reference.trimmed().isEmpty())
        return;

    const QString resolvedPath =
        Project::resolveReference(projectRoot, reference);
    const QFileInfo info(resolvedPath);
    if (!info.exists()) {
        addCollectionCheckIssue(
            &result->warnings,
            collection,
            QStringLiteral("%1 points to missing file \"%2\".")
                .arg(fieldName, QDir::toNativeSeparators(resolvedPath)));
        return;
    }
    if (!info.isFile()) {
        addCollectionCheckIssue(
            &result->warnings,
            collection,
            QStringLiteral("%1 points to \"%2\", but it is not a file.")
                .arg(fieldName, QDir::toNativeSeparators(resolvedPath)));
    }
}

CollectionCheckResult checkRegisteredCollections(
    const QString &projectRoot,
    const QVector<Collection> &collections)
{
    CollectionCheckResult result;
    result.checkedCollections = collections.size();

    for (const Collection &collection : collections) {
        const QFileInfo collectionDir(collection.dirPath);
        const QFileInfo yamlFile(
            QDir(collection.dirPath).filePath("collection.yaml"));

        if (!collectionDir.exists() || !collectionDir.isDir()) {
            addCollectionCheckIssue(
                &result.errors,
                collection,
                "Collection folder does not exist.");
            continue;
        }
        if (!yamlFile.exists() || !yamlFile.isFile()) {
            addCollectionCheckIssue(
                &result.errors,
                collection,
                "collection.yaml is missing.");
        }

        if (collection.collectionClass.trimmed().isEmpty()) {
            addCollectionCheckIssue(
                &result.errors,
                collection,
                "collection.yaml does not declare a class.");
            continue;
        }
        if (!collectionClassIsKnown(collection.collectionClass)) {
            addCollectionCheckIssue(
                &result.errors,
                collection,
                QStringLiteral("Unknown collection class \"%1\".")
                    .arg(collection.collectionClass));
            continue;
        }

        QString aliasError;
        if (!Project::collectionAliasIsSupported(
                collection.alias, &aliasError)) {
            addCollectionCheckIssue(
                &result.warnings,
                collection,
                QStringLiteral("Alias is not supported: %1").arg(aliasError));
        }

        const bool parentRequired =
            collection.collectionClass == "point_collection"
            || collection.collectionClass == "segmentation_collection";
        const QStringList parentClasses =
            compatibleParentClassesForCheck(collection.collectionClass);
        if (!parentClasses.isEmpty() || parentRequired) {
            checkCollectionReferenceToCollection(
                projectRoot,
                collections,
                collection,
                "Parent",
                collection.parent,
                parentClasses,
                parentRequired,
                &result);
        } else if (!collection.parent.trimmed().isEmpty()) {
            addCollectionCheckIssue(
                &result.warnings,
                collection,
                "Parent is set, but this collection class does not use it.");
        }

        const QStringList relatedClasses =
            compatibleRelatedClassesForCheck(collection.collectionClass);
        if (!relatedClasses.isEmpty()) {
            checkCollectionReferenceToCollection(
                projectRoot,
                collections,
                collection,
                "Related",
                collection.related,
                relatedClasses,
                false,
                &result);
        } else if (!collection.related.trimmed().isEmpty()) {
            addCollectionCheckIssue(
                &result.warnings,
                collection,
                "Related is set, but this collection class does not use it.");
        }

        if (collection.collectionClass == "preprocessed_image_collection"
            || collection.collectionClass == "other") {
            checkCollectionReferenceToFile(
                projectRoot,
                collection,
                "Registry",
                collection.registry,
                &result);
        } else if (!collection.registry.trimmed().isEmpty()) {
            addCollectionCheckIssue(
                &result.warnings,
                collection,
                "Registry is set, but this collection class does not use it.");
        }

        checkCollectionReferenceToFile(
            projectRoot, collection, "Script", collection.script, &result);
    }

    std::sort(result.errors.begin(), result.errors.end(),
              [](const CollectionCheckIssue &left,
                 const CollectionCheckIssue &right) {
                  const int collectionCompare = QString::compare(
                      left.collectionLabel,
                      right.collectionLabel,
                      Qt::CaseInsensitive);
                  if (collectionCompare != 0)
                      return collectionCompare < 0;
                  return QString::compare(
                      left.message,
                      right.message,
                      Qt::CaseInsensitive) < 0;
              });
    std::sort(result.warnings.begin(), result.warnings.end(),
              [](const CollectionCheckIssue &left,
                 const CollectionCheckIssue &right) {
                  const int collectionCompare = QString::compare(
                      left.collectionLabel,
                      right.collectionLabel,
                      Qt::CaseInsensitive);
                  if (collectionCompare != 0)
                      return collectionCompare < 0;
                  return QString::compare(
                      left.message,
                      right.message,
                      Qt::CaseInsensitive) < 0;
              });

    return result;
}

QString collectionCheckReportText(const CollectionCheckResult &result)
{
    QStringList lines;
    lines.append("Collection check");
    lines.append(QStringLiteral("Checked registered collections: %1")
                     .arg(result.checkedCollections));
    lines.append(QString());

    if (result.errors.isEmpty() && result.warnings.isEmpty()) {
        lines.append("No collection problems were found.");
        return lines.join('\n');
    }

    auto appendIssues = [&lines](const QString &title,
                                 const QVector<CollectionCheckIssue> &issues) {
        if (issues.isEmpty())
            return;
        lines.append(QStringLiteral("%1 (%2)").arg(title).arg(issues.size()));
        for (const CollectionCheckIssue &issue : issues) {
            lines.append(QStringLiteral("- %1: %2")
                             .arg(issue.collectionLabel, issue.message));
        }
        lines.append(QString());
    };

    appendIssues("Errors", result.errors);
    appendIssues("Warnings", result.warnings);
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return lines.join('\n');
}

void showCollectionCheckDialog(QWidget *parent,
                               const CollectionCheckResult &result)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Check collections");
    dialog.setMinimumSize(700, 420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(
        "This check reports broken collection contracts and missing "
        "references. It does not modify the project.",
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *report = new QTextBrowser(&dialog);
    report->setPlainText(collectionCheckReportText(result));
    report->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(report);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);

    dialog.exec();
}

struct RenameCollectionDecision
{
    bool accepted = false;
    QString newName;
};

bool collectionReferenceTouchesDirectory(const QString &projectRoot,
                                         const Collection &collection,
                                         const QString &directoryPath,
                                         QStringList *fields)
{
    QStringList touched;
    if (referenceIsInsideDirectory(
            projectRoot, collection.parent, directoryPath)) {
        touched.append("parent");
    }
    if (referenceIsInsideDirectory(
            projectRoot, collection.related, directoryPath)) {
        touched.append("related");
    }
    if (referenceIsInsideDirectory(
            projectRoot, collection.registry, directoryPath)) {
        touched.append("registry");
    }
    if (referenceIsInsideDirectory(
            projectRoot, collection.script, directoryPath)) {
        touched.append("script");
    }

    if (fields)
        *fields = touched;
    return !touched.isEmpty();
}

QStringList collectionRenameReferenceLabels(
    const QString &projectRoot,
    const QVector<Collection> &collections,
    const QString &oldDirectoryPath)
{
    QStringList labels;
    for (const Collection &collection : collections) {
        QStringList fields;
        if (!collectionReferenceTouchesDirectory(
                projectRoot, collection, oldDirectoryPath, &fields)) {
            continue;
        }
        labels.append(QStringLiteral("- %1: %2")
                          .arg(collectionDisplayLabel(collection),
                               fields.join(", ")));
    }
    labels.sort(Qt::CaseInsensitive);
    return labels;
}

bool widgetHasLayerInsideDirectory(QWidget *widget,
                                   const QString &directoryPath)
{
    auto *splitter = qobject_cast<QSplitter*>(widget);
    if (!splitter)
        return false;

    for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
        auto *viewer = qobject_cast<ImageViewer*>(
            splitter->widget(panelIndex));
        if (!viewer || !viewer->layerManager())
            continue;

        LayerManager *manager = viewer->layerManager();
        for (int layerIndex = 0; layerIndex < manager->layerCount();
             ++layerIndex) {
            Layer *layer = manager->layerAt(layerIndex);
            if (layer
                && normalizedPathIsInside(
                    layer->sourcePath(), directoryPath)) {
                return true;
            }
        }
    }
    return false;
}

QVector<int> workspaceTabIndexesTouchingDirectory(QTabWidget *tabs,
                                                  const QString &directoryPath)
{
    QVector<int> indexes;
    if (!tabs)
        return indexes;

    for (int index = 0; index < tabs->count(); ++index) {
        if (widgetHasLayerInsideDirectory(tabs->widget(index), directoryPath))
            indexes.append(index);
    }
    return indexes;
}

QStringList workspaceTabLabelsTouchingDirectory(QTabWidget *tabs,
                                                const QString &directoryPath)
{
    QStringList labels;
    const QVector<int> indexes =
        workspaceTabIndexesTouchingDirectory(tabs, directoryPath);
    for (const int index : indexes) {
        labels.append(QStringLiteral("- %1").arg(tabs->tabText(index)));
    }
    labels.sort(Qt::CaseInsensitive);
    return labels;
}

bool pathsAreSameForRename(const QString &left, const QString &right)
{
    return normalizedAbsolutePathForComparison(left)
        == normalizedAbsolutePathForComparison(right);
}

QString validateCollectionRenameFolderName(const QString &projectRoot,
                                           const QString &oldName,
                                           const QString &newName)
{
    const QString cleanName = newName.trimmed();
    if (cleanName.isEmpty())
        return "Folder name cannot be empty.";
    if (cleanName == "." || cleanName == "..")
        return "Folder name cannot be \".\" or \"..\".";
    if (cleanName.contains('/') || cleanName.contains('\\'))
        return "Enter a folder name, not a path.";
    for (const QChar character : cleanName) {
        if (character.unicode() < 32) {
            return "Folder name cannot contain control characters.";
        }
    }
    const QString invalidCharacters = QStringLiteral("<>:\"|?*");
    for (const QChar character : invalidCharacters) {
        if (cleanName.contains(character)) {
            return QStringLiteral("Folder name cannot contain \"%1\".")
                .arg(character);
        }
    }
    if (Project::reservedDirectories().contains(
            cleanName, Qt::CaseInsensitive)) {
        return "Folder name is reserved by rcaiman-projects.";
    }
    if (cleanName == oldName)
        return "Enter a different folder name.";

    const QString oldPath = QDir(projectRoot).filePath(oldName);
    const QString newPath = QDir(projectRoot).filePath(cleanName);
    if (QFileInfo::exists(newPath)
        && !pathsAreSameForRename(oldPath, newPath)) {
        return "A folder or file with that name already exists.";
    }
    return {};
}

QString collectionRenameReport(
    const QStringList &metadataReferences,
    const QStringList &scriptMentions,
    const QStringList &openTabs)
{
    QStringList lines;
    lines.append("Rename impact");
    lines.append(QString());

    if (metadataReferences.isEmpty()) {
        lines.append("No collection metadata references need updating.");
        lines.append(QString());
    } else {
        lines.append("Collection metadata references that will be updated:");
        lines.append(metadataReferences);
        lines.append(QString());
    }

    if (scriptMentions.isEmpty()) {
        lines.append("No scripts were found mentioning this folder.");
        lines.append(QString());
    } else {
        lines.append("Scripts that mention the old folder name or path:");
        lines.append(scriptMentions);
        lines.append("Scripts are not edited automatically.");
        lines.append(QString());
    }

    if (openTabs.isEmpty()) {
        lines.append("No open viewer tabs use files from this collection.");
    } else {
        lines.append("Open viewer tabs that will be closed before renaming:");
        lines.append(openTabs);
    }

    return lines.join('\n');
}

RenameCollectionDecision confirmRenameCollection(
    QWidget *parent,
    const QString &projectRoot,
    const Collection &collection,
    const QStringList &metadataReferences,
    const QStringList &scriptMentions,
    const QStringList &openTabs)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Rename collection folder");
    dialog.setMinimumWidth(680);

    auto *layout = new QVBoxLayout(&dialog);

    auto *intro = new QLabel(
        "Rename the collection folder. The collection class, files, "
        "collection.yaml, and style.yaml stay with the folder.",
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    auto *currentName = new QLineEdit(collection.dirName, &dialog);
    currentName->setReadOnly(true);
    form->addRow("Current folder:", currentName);

    auto *newName = new QLineEdit(collection.dirName, &dialog);
    newName->selectAll();
    form->addRow("New folder:", newName);
    layout->addLayout(form);

    auto *validationLabel = new QLabel(&dialog);
    validationLabel->setWordWrap(true);
    validationLabel->setStyleSheet("color: #9b1c1c;");
    layout->addWidget(validationLabel);

    auto *report = new QTextBrowser(&dialog);
    report->setPlainText(
        collectionRenameReport(metadataReferences, scriptMentions, openTabs));
    report->setMinimumHeight(230);
    report->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(report);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Rename");
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    layout->addWidget(buttons);

    auto updateValidation = [&]() {
        const QString error = validateCollectionRenameFolderName(
            projectRoot, collection.dirName, newName->text());
        validationLabel->setText(error);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
    };
    QObject::connect(newName, &QLineEdit::textChanged,
                     &dialog, updateValidation);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);
    updateValidation();

    RenameCollectionDecision decision;
    if (dialog.exec() != QDialog::Accepted)
        return decision;

    decision.accepted = true;
    decision.newName = newName->text().trimmed();
    return decision;
}

QString remapReferenceInsideDirectory(const QString &projectRoot,
                                      const QString &reference,
                                      const QString &oldDirectoryPath,
                                      const QString &newDirectoryPath)
{
    if (reference.trimmed().isEmpty())
        return reference;

    const QString resolvedPath =
        Project::resolveReference(projectRoot, reference);
    if (!normalizedPathIsInside(resolvedPath, oldDirectoryPath))
        return reference;

    const QString relativeInside =
        QDir(oldDirectoryPath).relativeFilePath(resolvedPath);
    const QString remappedPath = relativeInside == "."
        ? newDirectoryPath
        : QDir(newDirectoryPath).filePath(relativeInside);
    return Project::portableReference(projectRoot, remappedPath);
}

bool updateCollectionReferencesAfterRename(
    const QString &projectRoot,
    const QVector<Collection> &collections,
    const QString &oldDirectoryPath,
    const QString &newDirectoryPath,
    const QString &newName,
    int *updatedCollections,
    QString *errorMessage)
{
    if (updatedCollections)
        *updatedCollections = 0;

    int written = 0;
    for (Collection collection : collections) {
        const bool isRenamedCollection =
            normalizedAbsolutePathForComparison(collection.dirPath)
            == normalizedAbsolutePathForComparison(oldDirectoryPath);
        if (isRenamedCollection) {
            collection.dirPath = newDirectoryPath;
            collection.dirName = newName;
        }

        const QString parent = remapReferenceInsideDirectory(
            projectRoot, collection.parent, oldDirectoryPath,
            newDirectoryPath);
        const QString related = remapReferenceInsideDirectory(
            projectRoot, collection.related, oldDirectoryPath,
            newDirectoryPath);
        const QString registry = remapReferenceInsideDirectory(
            projectRoot, collection.registry, oldDirectoryPath,
            newDirectoryPath);
        const QString script = remapReferenceInsideDirectory(
            projectRoot, collection.script, oldDirectoryPath,
            newDirectoryPath);

        if (parent == collection.parent
            && related == collection.related
            && registry == collection.registry
            && script == collection.script) {
            continue;
        }

        if (!Project::updateCollectionMetadata(
                projectRoot,
                collection.dirPath,
                collection.alias,
                parent,
                related,
                registry,
                script,
                errorMessage)) {
            return false;
        }
        ++written;
    }

    if (updatedCollections)
        *updatedCollections = written;
    return true;
}

bool renameProjectChildDirectory(const QString &projectRoot,
                                 const QString &oldName,
                                 const QString &newName,
                                 QString *errorMessage)
{
    QDir root(projectRoot);
    const QString oldPath = root.filePath(oldName);
    const QString newPath = root.filePath(newName);

    if (!pathsAreSameForRename(oldPath, newPath)) {
        if (root.rename(oldName, newName))
            return true;
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Could not rename \"%1\" to \"%2\".")
                .arg(oldName, newName);
        }
        return false;
    }

    QString temporaryName =
        QStringLiteral("%1.rename_tmp_%2")
            .arg(oldName,
                 QString::number(QDateTime::currentMSecsSinceEpoch()));
    while (QFileInfo::exists(root.filePath(temporaryName))) {
        temporaryName.append('x');
    }

    if (!root.rename(oldName, temporaryName)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Could not prepare case-only rename for \"%1\".")
                .arg(oldName);
        }
        return false;
    }
    if (!root.rename(temporaryName, newName)) {
        root.rename(temporaryName, oldName);
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Could not rename \"%1\" to \"%2\".")
                .arg(oldName, newName);
        }
        return false;
    }
    return true;
}

QVector<QString> tiffFilesWithBaseName(const QString &directoryPath,
                                       const QString &baseName)
{
    QVector<QString> matches;
    const QDir directory(directoryPath);
    const QFileInfoList files =
        directory.entryInfoList(QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &file : files) {
        const QString suffix = file.suffix().toLower();
        if (suffix != "tif" && suffix != "tiff")
            continue;
        if (file.completeBaseName().compare(
                baseName, Qt::CaseInsensitive) == 0) {
            matches.append(file.absoluteFilePath());
        }
    }
    std::sort(matches.begin(), matches.end(),
              [](const QString &left, const QString &right) {
                  return QString::compare(
                      left, right, Qt::CaseInsensitive) < 0;
              });
    return matches;
}

QVector<MaskCandidate> compatibleMaskCandidates(
    const QString &projectRoot,
    const Collection &pointCollection,
    const QVector<Collection> &collections,
    const QString &baseName = {})
{
    QVector<MaskCandidate> candidates;
    const QString pointParent =
        normalizedProjectReference(projectRoot, pointCollection.parent);
    const QString pointRelated =
        normalizedProjectReference(projectRoot, pointCollection.related);
    if (pointParent.isEmpty() && pointRelated.isEmpty())
        return candidates;

    for (const Collection &collection : collections) {
        if (collection.collectionClass != "segmentation_collection")
            continue;

        const QString maskParent =
            normalizedProjectReference(projectRoot, collection.parent);
        const QString maskCollectionPath =
            normalizedAbsolutePathForComparison(collection.dirPath);
        const bool relatedMatch =
            !pointRelated.isEmpty()
            && pointRelated == maskCollectionPath;
        const bool parentMatch =
            !pointParent.isEmpty()
            && !maskParent.isEmpty()
            && maskParent == pointParent;
        if (!relatedMatch && !parentMatch)
            continue;

        QVector<QString> files;
        if (baseName.isEmpty()) {
            const QFileInfoList entries = QDir(collection.dirPath).entryInfoList(
                {"*.tif", "*.tiff", "*.TIF", "*.TIFF"},
                QDir::Files | QDir::NoSymLinks,
                QDir::Name);
            for (const QFileInfo &file : entries)
                files.append(file.absoluteFilePath());
        } else {
            files = tiffFilesWithBaseName(collection.dirPath, baseName);
        }

        for (const QString &path : files) {
            candidates.append(MaskCandidate{
                collection,
                path,
                QStringLiteral("%1/%2")
                    .arg(collection.dirName, QFileInfo(path).fileName())
            });
        }
    }
    return candidates;
}

bool validateBinaryMask(const RasterData &mask, QString *errorMessage = nullptr)
{
    if (mask.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The selected mask is empty.");
        return false;
    }
    if (mask.bands() != 1
        || mask.dataType() != RasterData::DataType::UInt8) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The selected mask must be a single-band UInt8 TIFF.");
        return false;
    }

    const quint8 *maskValues = mask.bandPointer<quint8>(0);
    if (!maskValues) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "The selected mask could not be inspected.");
        return false;
    }

    const qsizetype pixelCount =
        static_cast<qsizetype>(mask.width()) * mask.height();
    for (qsizetype offset = 0; offset < pixelCount; ++offset) {
        const int value = maskValues[offset];
        if (value != 0 && value != 1 && value != 255) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "The selected mask is not binary.\n\n"
                    "Binary masks must contain only 0/255 values, "
                    "or legacy 0/1 values.");
            }
            return false;
        }
    }
    return true;
}

class ProjectExplorerSortProxyModel : public QAbstractProxyModel
{
public:
    explicit ProjectExplorerSortProxyModel(QObject *parent = nullptr)
        : QAbstractProxyModel(parent)
    {
    }

    void setSourceModel(QAbstractItemModel *source) override
    {
        if (source == sourceModel())
            return;

        if (sourceModel())
            disconnect(sourceModel(), nullptr, this, nullptr);

        beginResetModel();
        QAbstractProxyModel::setSourceModel(source);
        clearCaches();
        endResetModel();

        if (!source)
            return;

        connect(source, &QAbstractItemModel::modelReset,
                this, [this]() { invalidate(); });
        connect(source, &QAbstractItemModel::layoutChanged,
                this, [this](const QList<QPersistentModelIndex>&,
                             QAbstractItemModel::LayoutChangeHint) {
                    refreshSourceLayout();
                });
        connect(source, &QAbstractItemModel::rowsInserted,
                this, [this](const QModelIndex &parent, int, int) {
                    refreshSourceBranch(parent);
                });
        connect(source, &QAbstractItemModel::rowsRemoved,
                this, [this](const QModelIndex &parent, int, int) {
                    refreshSourceBranch(parent);
                });
        connect(source, &QAbstractItemModel::dataChanged,
                this, [this](const QModelIndex &topLeft,
                             const QModelIndex &bottomRight,
                             const QList<int> &roles) {
                    refreshSourceData(topLeft, bottomRight, roles);
                });
    }

    void setSortMode(ProjectExplorerSortMode mode)
    {
        if (m_sortMode == mode)
            return;
        m_sortMode = mode;
        invalidate();
    }

    void setStarredOnly(bool enabled)
    {
        if (m_starredOnly == enabled)
            return;
        m_starredOnly = enabled;
        invalidate();
    }

    void setProjectRootPath(const QString &path)
    {
        m_projectRootPath = normalizedPath(path);
        invalidate();
    }

    void invalidate()
    {
        beginResetModel();
        clearCaches();
        endResetModel();
    }

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override
    {
        if (!proxyIndex.isValid() || !sourceModel())
            return {};

        const ProxyNode *node = nodeFromIndex(proxyIndex);
        if (!node || node->kind != ProxyNode::Kind::Source)
            return {};

        const QModelIndex sourceIndex = node->sourceIndex;
        return sourceIndex.isValid()
            ? sourceIndex.sibling(sourceIndex.row(), proxyIndex.column())
            : QModelIndex();
    }

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override
    {
        if (!sourceIndex.isValid() || !sourceModel())
            return {};

        const QModelIndex rowSource =
            sourceIndex.sibling(sourceIndex.row(), 0);
        if (!filterAcceptsSourceIndex(rowSource))
            return {};

        const int row = rowForSource(rowSource);
        if (row < 0)
            return {};

        return createIndex(
            row, sourceIndex.column(), nodeForSource(rowSource));
    }

    QModelIndex index(int row,
                      int column,
                      const QModelIndex &parent = {}) const override
    {
        if (!sourceModel() || row < 0 || column < 0)
            return {};
        if (parent.isValid() && parent.column() != 0)
            return {};
        if (column >= columnCount(parent))
            return {};

        const ProxyNode *parentNode = nodeFromIndex(parent);
        if (parentNode && parentNode->kind == ProxyNode::Kind::Group) {
            const QVector<QPersistentModelIndex> &children =
                sourceChildrenForGroup(parentNode->group);
            if (row >= children.size())
                return {};
            return createIndex(
                row, column, nodeForSource(children.at(row)));
        }

        if (!parent.isValid() && !m_projectRootPath.isEmpty()) {
            const QModelIndex sourceRoot = projectRootSourceIndex();
            if (row != 0 || !sourceRoot.isValid())
                return {};
            return createIndex(
                row, column, nodeForSource(sourceRoot));
        }

        const QModelIndex sourceParent = parent.isValid()
            ? mapToSource(parent)
            : QModelIndex();
        if (sourceParent.isValid() && isProjectRootSource(sourceParent)) {
            const QVector<RootGroup> &groups = visibleRootGroups();
            if (row < groups.size())
                return createIndex(
                    row, column, nodeForGroup(groups.at(row)));

            const QVector<QPersistentModelIndex> &files =
                directRootFiles();
            const int fileRow = row - groups.size();
            if (fileRow >= files.size())
                return {};
            return createIndex(
                row, column, nodeForSource(files.at(fileRow)));
        }

        const QVector<QPersistentModelIndex> &children =
            sourceChildrenForParent(sourceParent);
        if (row >= children.size())
            return {};
        return createIndex(row, column, nodeForSource(children.at(row)));
    }

    QModelIndex parent(const QModelIndex &child) const override
    {
        if (!child.isValid() || !sourceModel())
            return {};

        const ProxyNode *node = nodeFromIndex(child);
        if (!node)
            return {};

        if (node->kind == ProxyNode::Kind::Group)
            return projectRootProxyIndex();

        const QModelIndex sourceIndex = node->sourceIndex;
        if (!sourceIndex.isValid() || isProjectRootSource(sourceIndex))
            return {};

        const RootGroup group = rootGroupForSource(sourceIndex);
        if (group != RootGroup::None)
            return groupIndex(group);

        const QModelIndex sourceParent = sourceIndex.parent();
        if (!sourceParent.isValid())
            return {};
        return mapFromSource(sourceParent);
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        if (!sourceModel())
            return 0;
        if (parent.isValid() && parent.column() != 0)
            return 0;

        const ProxyNode *parentNode = nodeFromIndex(parent);
        if (parentNode && parentNode->kind == ProxyNode::Kind::Group)
            return sourceChildrenForGroup(parentNode->group).size();

        if (!parent.isValid() && !m_projectRootPath.isEmpty())
            return projectRootSourceIndex().isValid() ? 1 : 0;

        const QModelIndex sourceParent = parent.isValid()
            ? mapToSource(parent)
            : QModelIndex();
        if (sourceParent.isValid() && isProjectRootSource(sourceParent)) {
            return visibleRootGroups().size() + directRootFiles().size();
        }

        return sourceChildrenForParent(sourceParent).size();
    }

    int columnCount(const QModelIndex &parent = {}) const override
    {
        if (!sourceModel())
            return 0;

        const ProxyNode *parentNode = nodeFromIndex(parent);
        if (parentNode && parentNode->kind == ProxyNode::Kind::Group)
            return sourceModel()->columnCount(projectRootSourceIndex());

        const QModelIndex sourceParent = parent.isValid()
            ? mapToSource(parent)
            : QModelIndex();
        return sourceModel()->columnCount(sourceParent);
    }

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return {};

        const ProxyNode *node = nodeFromIndex(index);
        if (!node)
            return {};

        if (role == ProjectExplorerGroupRole)
            return node->kind == ProxyNode::Kind::Group;
        if (role == ProjectExplorerGroupIdRole)
            return node->kind == ProxyNode::Kind::Group
                ? groupId(node->group)
                : QString();

        if (node->kind == ProxyNode::Kind::Group)
            return groupData(node->group, index.column(), role);

        return sourceModel()
            ? sourceModel()->data(mapToSource(index), role)
            : QVariant();
    }

    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override
    {
        return sourceModel()
            ? sourceModel()->headerData(section, orientation, role)
            : QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid() || !sourceModel())
            return {};

        const ProxyNode *node = nodeFromIndex(index);
        if (!node)
            return {};
        if (node->kind == ProxyNode::Kind::Group)
            return Qt::ItemIsEnabled;

        return sourceModel()->flags(mapToSource(index));
    }

    bool hasChildren(const QModelIndex &parent = {}) const override
    {
        if (!sourceModel())
            return false;

        const ProxyNode *parentNode = nodeFromIndex(parent);
        if (parentNode && parentNode->kind == ProxyNode::Kind::Group)
            return rowCount(parent) > 0;

        const QModelIndex sourceParent = parent.isValid()
            ? mapToSource(parent)
            : QModelIndex();
        if (sourceParent.isValid() && isProjectRootSource(sourceParent))
            return rowCount(parent) > 0;

        return sourceModel()->hasChildren(sourceParent);
    }

    bool canFetchMore(const QModelIndex &parent) const override
    {
        if (!sourceModel())
            return false;

        const ProxyNode *node = nodeFromIndex(parent);
        if (node && node->kind == ProxyNode::Kind::Group)
            return false;

        return sourceModel()->canFetchMore(mapToSource(parent));
    }

    void fetchMore(const QModelIndex &parent) override
    {
        if (!sourceModel())
            return;

        const ProxyNode *node = nodeFromIndex(parent);
        if (node && node->kind == ProxyNode::Kind::Group)
            return;

        const QModelIndex sourceParent = mapToSource(parent);
        m_childrenCache.remove(childrenCacheKey(sourceParent));
        sourceModel()->fetchMore(sourceParent);
    }

    QStringList mimeTypes() const override
    {
        return sourceModel() ? sourceModel()->mimeTypes() : QStringList();
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        if (!sourceModel())
            return nullptr;

        QModelIndexList sourceIndexes;
        QSet<QString> seenRows;
        for (const QModelIndex &index : indexes) {
            if (!index.isValid() || index.column() != 0)
                continue;
            if (index.data(ProjectExplorerGroupRole).toBool())
                continue;

            const QModelIndex sourceIndex = mapToSource(index);
            if (!sourceIndex.isValid())
                continue;

            const QString key = sourceIndexKey(sourceIndex);
            if (seenRows.contains(key))
                continue;
            seenRows.insert(key);
            sourceIndexes.append(sourceIndex);
        }
        return sourceIndexes.isEmpty()
            ? nullptr
            : sourceModel()->mimeData(sourceIndexes);
    }

    bool canDropMimeData(const QMimeData *data,
                         Qt::DropAction action,
                         int row,
                         int column,
                         const QModelIndex &parent) const override
    {
        if (!sourceModel())
            return false;

        const ProxyNode *node = nodeFromIndex(parent);
        if (node && node->kind == ProxyNode::Kind::Group)
            return false;

        return sourceModel()->canDropMimeData(
            data, action, row, column, mapToSource(parent));
    }

    bool dropMimeData(const QMimeData *data,
                      Qt::DropAction action,
                      int row,
                      int column,
                      const QModelIndex &parent) override
    {
        if (!sourceModel())
            return false;

        const ProxyNode *node = nodeFromIndex(parent);
        if (node && node->kind == ProxyNode::Kind::Group)
            return false;

        return sourceModel()->dropMimeData(
            data, action, row, column, mapToSource(parent));
    }

    Qt::DropActions supportedDragActions() const override
    {
        return sourceModel()
            ? sourceModel()->supportedDragActions()
            : Qt::DropActions();
    }

    Qt::DropActions supportedDropActions() const override
    {
        return sourceModel()
            ? sourceModel()->supportedDropActions()
            : Qt::DropActions();
    }

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override
    {
        Q_UNUSED(column);
        if (m_sortOrder == order)
            return;
        m_sortOrder = order;
        invalidate();
    }

private:
    enum class RootGroup {
        None = 0,
        Reserved,
        Collections,
        RegularFolders
    };

    struct ProxyNode
    {
        enum class Kind {
            Source,
            Group
        };

        Kind kind = Kind::Source;
        QPersistentModelIndex sourceIndex;
        RootGroup group = RootGroup::None;
    };

    static QString normalizedPath(const QString &path)
    {
        if (path.isEmpty())
            return {};
        QString normalized = QDir::cleanPath(
            QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
        normalized = normalized.toCaseFolded();
#endif
        return normalized;
    }

    static QString sourceIndexKey(const QModelIndex &index)
    {
        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(index.model());
        if (model)
            return model->filePath(index);
        return QStringLiteral("%1:%2:%3")
            .arg(index.row())
            .arg(index.column())
            .arg(reinterpret_cast<quintptr>(index.internalPointer()));
    }

    const ProxyNode *nodeFromIndex(const QModelIndex &index) const
    {
        return index.isValid()
            ? static_cast<const ProxyNode*>(index.internalPointer())
            : nullptr;
    }

    ProxyNode *nodeForSource(const QModelIndex &sourceIndex) const
    {
        const QModelIndex rowSource =
            sourceIndex.sibling(sourceIndex.row(), 0);
        const QString key = sourceIndexKey(rowSource);
        if (ProxyNode *node = m_sourceNodes.value(key, nullptr))
            return node;

        auto node = std::make_unique<ProxyNode>();
        node->kind = ProxyNode::Kind::Source;
        node->sourceIndex = rowSource;
        ProxyNode *rawNode = node.get();
        m_nodes.push_back(std::move(node));
        m_sourceNodes.insert(key, rawNode);
        return rawNode;
    }

    ProxyNode *nodeForGroup(RootGroup group) const
    {
        const int key = static_cast<int>(group);
        if (ProxyNode *node = m_groupNodes.value(key, nullptr))
            return node;

        auto node = std::make_unique<ProxyNode>();
        node->kind = ProxyNode::Kind::Group;
        node->group = group;
        ProxyNode *rawNode = node.get();
        m_nodes.push_back(std::move(node));
        m_groupNodes.insert(key, rawNode);
        return rawNode;
    }

    void clearNodes()
    {
        m_sourceNodes.clear();
        m_groupNodes.clear();
        m_nodes.clear();
    }

    void clearCaches()
    {
        clearNodes();
        clearLayoutCaches();
    }

    void clearLayoutCaches()
    {
        m_rootLayoutValid = false;
        m_visibleRootGroupsCache.clear();
        m_groupChildrenCache.clear();
        m_directRootFilesCache.clear();
        m_childrenCache.clear();
    }

    QModelIndex projectRootSourceIndex() const
    {
        if (!sourceModel() || m_projectRootPath.isEmpty())
            return {};

        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        return model ? model->index(m_projectRootPath) : QModelIndex();
    }

    QModelIndex projectRootProxyIndex() const
    {
        const QModelIndex sourceRoot = projectRootSourceIndex();
        if (!sourceRoot.isValid())
            return {};

        return createIndex(0, 0, nodeForSource(sourceRoot));
    }

    QModelIndex groupIndex(RootGroup group) const
    {
        const QVector<RootGroup> &groups = visibleRootGroups();
        const int row = groups.indexOf(group);
        if (row < 0)
            return {};
        return createIndex(row, 0, nodeForGroup(group));
    }

    bool isProjectRootOrAncestor(const QString &path) const
    {
        if (m_projectRootPath.isEmpty())
            return false;

        const QString candidate = normalizedPath(path);
        if (candidate.isEmpty())
            return false;
        if (candidate == m_projectRootPath)
            return true;

        QString ancestor = m_projectRootPath;
        while (!ancestor.isEmpty()) {
            const QString parent = QFileInfo(ancestor).absoluteDir().absolutePath();
            const QString normalizedParent = normalizedPath(parent);
            if (normalizedParent == ancestor)
                break;
            ancestor = normalizedParent;
            if (candidate == ancestor)
                return true;
        }
        return false;
    }

    bool isProjectRootSource(const QModelIndex &sourceIndex) const
    {
        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        if (!model || !sourceIndex.isValid() || m_projectRootPath.isEmpty())
            return false;
        return normalizedPath(model->filePath(sourceIndex))
            == m_projectRootPath;
    }

    bool isDirectProjectRootChildSource(
        const QModelIndex &sourceIndex) const
    {
        return sourceIndex.isValid()
            && sourceIndex.parent().isValid()
            && isProjectRootSource(sourceIndex.parent());
    }

    RootGroup rootGroupForSource(const QModelIndex &sourceIndex) const
    {
        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        if (!model || !sourceIndex.isValid() || m_projectRootPath.isEmpty())
            return RootGroup::None;

        const QFileInfo info = model->fileInfo(sourceIndex);
        if (!info.isDir())
            return RootGroup::None;

        const QString parentPath =
            normalizedPath(info.absoluteDir().absolutePath());
        if (parentPath != m_projectRootPath)
            return RootGroup::None;

        if (Project::reservedDirectories().contains(
                info.fileName(), Qt::CaseInsensitive)) {
            return RootGroup::Reserved;
        }

        if (!model->collectionClassForDirectory(
                info.absoluteFilePath()).isEmpty()) {
            return RootGroup::Collections;
        }

        return RootGroup::RegularFolders;
    }

    bool filterAcceptsSourceIndex(const QModelIndex &sourceIndex) const
    {
        if (!m_starredOnly)
            return true;

        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        if (!model || !sourceIndex.isValid())
            return sourceIndex.isValid();

        const QFileInfo info = model->fileInfo(sourceIndex);
        if (isProjectRootOrAncestor(info.absoluteFilePath()))
            return true;

        return model->isMarkedOrInherited(info)
            || model->hasMarkedDescendant(info);
    }

    QString childrenCacheKey(const QModelIndex &sourceParent) const
    {
        return sourceParent.isValid()
            ? sourceIndexKey(sourceParent)
            : QStringLiteral("<source-root>");
    }

    void refreshSourceBranch(const QModelIndex &sourceParent)
    {
        if (!sourceParent.isValid() || isProjectRootSource(sourceParent)) {
            invalidate();
            return;
        }

        m_childrenCache.remove(childrenCacheKey(sourceParent));
        emit layoutAboutToBeChanged();
        emit layoutChanged();

        const QModelIndex proxyParent = mapFromSource(sourceParent);
        if (!proxyParent.isValid())
            return;

        const QModelIndex left = proxyParent.sibling(proxyParent.row(), 0);
        const QModelIndex right = proxyParent.sibling(
            proxyParent.row(), qMax(0, columnCount(proxyParent.parent()) - 1));
        emit dataChanged(left, right, {Qt::DisplayRole});
    }

    void refreshSourceData(const QModelIndex &topLeft,
                           const QModelIndex &bottomRight,
                           const QList<int> &roles)
    {
        if (!topLeft.isValid() || !bottomRight.isValid())
            return;

        const bool mayChangeRootGrouping =
            roles.isEmpty()
            || roles.contains(Qt::ForegroundRole)
            || roles.contains(Qt::ToolTipRole);
        if (mayChangeRootGrouping) {
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                const QModelIndex sourceIndex =
                    sourceModel()->index(row, 0, topLeft.parent());
                if (isProjectRootSource(sourceIndex)
                    || isDirectProjectRootChildSource(sourceIndex)) {
                    invalidate();
                    return;
                }
            }
        }

        const QModelIndex sourceParent = topLeft.parent();
        if (sourceParent.isValid())
            m_childrenCache.remove(childrenCacheKey(sourceParent));

        const QModelIndex proxyTopLeft = mapFromSource(topLeft);
        const QModelIndex proxyBottomRight = mapFromSource(bottomRight);
        if (proxyTopLeft.isValid() && proxyBottomRight.isValid())
            emit dataChanged(proxyTopLeft, proxyBottomRight, roles);
    }

    void refreshSourceLayout()
    {
        emit layoutAboutToBeChanged();
        clearLayoutCaches();
        emit layoutChanged();
    }

    const QVector<QPersistentModelIndex> &sourceChildrenForParent(
        const QModelIndex &sourceParent) const
    {
        if (!sourceModel())
            return m_emptyChildren;

        const QString key = childrenCacheKey(sourceParent);
        auto cached = m_childrenCache.constFind(key);
        if (cached != m_childrenCache.constEnd())
            return cached.value();

        QVector<QPersistentModelIndex> children;
        const int rows = sourceModel()->rowCount(sourceParent);
        children.reserve(rows);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex child =
                sourceModel()->index(row, 0, sourceParent);
            if (child.isValid() && filterAcceptsSourceIndex(child))
                children.append(child);
        }
        sortSourceChildren(children);
        auto inserted = m_childrenCache.insert(key, children);
        return inserted.value();
    }

    const QVector<QPersistentModelIndex> &sourceChildrenForGroup(
        RootGroup group) const
    {
        ensureRootLayout();
        const int key = static_cast<int>(group);
        auto it = m_groupChildrenCache.constFind(key);
        return it == m_groupChildrenCache.constEnd()
            ? m_emptyChildren
            : it.value();
    }

    const QVector<QPersistentModelIndex> &directRootFiles() const
    {
        ensureRootLayout();
        return m_directRootFilesCache;
    }

    const QVector<RootGroup> &visibleRootGroups() const
    {
        ensureRootLayout();
        return m_visibleRootGroupsCache;
    }

    void ensureRootLayout() const
    {
        if (m_rootLayoutValid)
            return;

        m_visibleRootGroupsCache.clear();
        m_groupChildrenCache.clear();
        m_directRootFilesCache.clear();

        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        const QModelIndex root = projectRootSourceIndex();
        if (!model || !root.isValid()) {
            m_rootLayoutValid = true;
            return;
        }

        for (RootGroup group : {RootGroup::Reserved,
                                RootGroup::Collections,
                                RootGroup::RegularFolders}) {
            m_groupChildrenCache.insert(
                static_cast<int>(group), QVector<QPersistentModelIndex>());
        }

        const int rows = sourceModel()->rowCount(root);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex child =
                sourceModel()->index(row, 0, root);
            if (!child.isValid())
                continue;

            if (!filterAcceptsSourceIndex(child))
                continue;

            const QFileInfo info = model->fileInfo(child);
            if (!info.isDir()) {
                m_directRootFilesCache.append(child);
                continue;
            }

            const RootGroup group = rootGroupForSource(child);
            if (group == RootGroup::None)
                continue;
            m_groupChildrenCache[static_cast<int>(group)].append(child);
        }

        sortSourceChildren(m_directRootFilesCache);
        for (RootGroup group : {RootGroup::Reserved,
                                RootGroup::Collections,
                                RootGroup::RegularFolders}) {
            QVector<QPersistentModelIndex> &children =
                m_groupChildrenCache[static_cast<int>(group)];
            sortSourceChildren(children);
            if (!children.isEmpty())
                m_visibleRootGroupsCache.append(group);
        }
        m_rootLayoutValid = true;
    }

    int rowForSource(const QModelIndex &sourceIndex) const
    {
        if (!sourceIndex.isValid())
            return -1;

        if (isProjectRootSource(sourceIndex))
            return 0;

        const RootGroup group = rootGroupForSource(sourceIndex);
        if (group != RootGroup::None)
            return indexOfSource(sourceChildrenForGroup(group), sourceIndex);

        const QModelIndex sourceParent = sourceIndex.parent();
        if (sourceParent.isValid() && isProjectRootSource(sourceParent)) {
            const int fileRow =
                indexOfSource(directRootFiles(), sourceIndex);
            return fileRow < 0
                ? -1
                : visibleRootGroups().size() + fileRow;
        }

        return indexOfSource(
            sourceChildrenForParent(sourceParent), sourceIndex);
    }

    int indexOfSource(
        const QVector<QPersistentModelIndex> &indexes,
        const QModelIndex &target) const
    {
        const QModelIndex rowTarget =
            target.sibling(target.row(), 0);
        for (int i = 0; i < indexes.size(); ++i) {
            if (QModelIndex(indexes.at(i)) == rowTarget)
                return i;
        }
        return -1;
    }

    void sortSourceChildren(
        QVector<QPersistentModelIndex> &children) const
    {
        std::sort(
            children.begin(),
            children.end(),
            [this](const QPersistentModelIndex &left,
                   const QPersistentModelIndex &right) {
                return m_sortOrder == Qt::AscendingOrder
                    ? sourceIndexLessThan(QModelIndex(left),
                                          QModelIndex(right))
                    : sourceIndexLessThan(QModelIndex(right),
                                          QModelIndex(left));
            });
    }

    bool sourceIndexLessThan(const QModelIndex &left,
                             const QModelIndex &right) const
    {
        const auto *model =
            qobject_cast<const ProjectFileSystemModel*>(sourceModel());
        if (!model)
            return left.row() < right.row();

        const QFileInfo leftInfo = model->fileInfo(left);
        const QFileInfo rightInfo = model->fileInfo(right);
        if (leftInfo.isDir() != rightInfo.isDir())
            return leftInfo.isDir();

        if (!leftInfo.isDir()
            && m_sortMode == ProjectExplorerSortMode::Type) {
            const int extensionCompare = QString::compare(
                leftInfo.suffix(), rightInfo.suffix(), Qt::CaseInsensitive);
            if (extensionCompare != 0)
                return extensionCompare < 0;
        }

        const int nameCompare = QString::compare(
            leftInfo.fileName(), rightInfo.fileName(), Qt::CaseInsensitive);
        if (nameCompare != 0)
            return nameCompare < 0;

        return QString::compare(
            leftInfo.absoluteFilePath(),
            rightInfo.absoluteFilePath(),
            Qt::CaseInsensitive) < 0;
    }

    QString labelForGroup(RootGroup group) const
    {
        switch (group) {
        case RootGroup::Reserved:
            return QStringLiteral("Reserved folders");
        case RootGroup::Collections:
            return QStringLiteral("Collections");
        case RootGroup::RegularFolders:
            return QStringLiteral("Regular folders");
        case RootGroup::None:
            break;
        }
        return {};
    }

    QString groupId(RootGroup group) const
    {
        switch (group) {
        case RootGroup::Reserved:
            return QStringLiteral("reserved");
        case RootGroup::Collections:
            return QStringLiteral("collections");
        case RootGroup::RegularFolders:
            return QStringLiteral("regular_folders");
        case RootGroup::None:
            break;
        }
        return {};
    }

    QString tooltipForGroup(RootGroup group) const
    {
        switch (group) {
        case RootGroup::Reserved:
            return QStringLiteral(
                "Project folders managed by rcaiman-projects.");
        case RootGroup::Collections:
            return QStringLiteral("Registered project collections.");
        case RootGroup::RegularFolders:
            return QStringLiteral("Folders not registered as collections.");
        case RootGroup::None:
            break;
        }
        return {};
    }

    QVariant groupData(RootGroup group, int column, int role) const
    {
        if (role == ProjectFileMarkedRole
            || role == ProjectFileDirectlyMarkedRole) {
            return false;
        }

        if (column != 0)
            return {};

        if (role == Qt::DisplayRole) {
            return QStringLiteral("%1 (%2)")
                .arg(labelForGroup(group))
                .arg(sourceChildrenForGroup(group).size());
        }
        if (role == Qt::ToolTipRole)
            return tooltipForGroup(group);
        if (role == Qt::FontRole) {
            QFont font;
            font.setBold(true);
            return font;
        }
        if (role == Qt::ForegroundRole)
            return QBrush(QColor("#243447"));
        if (role == Qt::BackgroundRole)
            return QBrush(QColor("#eef3f8"));
        if (role == Qt::DecorationRole)
            return {};
        if (role == Qt::SizeHintRole)
            return QSize(-1, 22);

        return {};
    }

    ProjectExplorerSortMode m_sortMode = ProjectExplorerSortMode::Name;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    bool m_starredOnly = false;
    QString m_projectRootPath;
    mutable std::vector<std::unique_ptr<ProxyNode>> m_nodes;
    mutable QHash<QString, ProxyNode*> m_sourceNodes;
    mutable QHash<int, ProxyNode*> m_groupNodes;
    mutable bool m_rootLayoutValid = false;
    mutable QVector<RootGroup> m_visibleRootGroupsCache;
    mutable QHash<int, QVector<QPersistentModelIndex>> m_groupChildrenCache;
    mutable QVector<QPersistentModelIndex> m_directRootFilesCache;
    mutable QHash<QString, QVector<QPersistentModelIndex>> m_childrenCache;
    mutable QVector<QPersistentModelIndex> m_emptyChildren;
};

constexpr int ProjectFileMarkAreaWidth = 24;
constexpr int ProjectFileMarkSize = 15;
constexpr int ProjectFileDetailAreaWidth = 104;
constexpr int ProjectFileDetailMinimumRowWidth = 260;
constexpr int MaskBrushSliderMaximum = 1000;
constexpr double MaskBrushSliderGamma = 2.5;

int maskBrushSizeFromSlider(int sliderValue, int maximumSize)
{
    maximumSize = qMax(1, maximumSize);
    const double t = qBound(
        0.0,
        static_cast<double>(sliderValue) / MaskBrushSliderMaximum,
        1.0);
    return qBound(
        1,
        static_cast<int>(
            std::round(1.0 + (maximumSize - 1.0)
                * std::pow(t, MaskBrushSliderGamma))),
        maximumSize);
}

int maskBrushSliderFromSize(int size, int maximumSize)
{
    maximumSize = qMax(1, maximumSize);
    size = qBound(1, size, maximumSize);
    if (maximumSize == 1)
        return 0;

    const double t = std::pow(
        (static_cast<double>(size) - 1.0) / (maximumSize - 1.0),
        1.0 / MaskBrushSliderGamma);
    return qBound(
        0,
        static_cast<int>(std::round(t * MaskBrushSliderMaximum)),
        MaskBrushSliderMaximum);
}

QRect projectFileMarkAreaRect(const QRect &rowRect)
{
    return QRect(rowRect.right() - ProjectFileMarkAreaWidth + 1,
                 rowRect.top(),
                 ProjectFileMarkAreaWidth,
                 rowRect.height());
}

QRect projectFileMarkIconRect(const QRect &rowRect)
{
    const QRect area = projectFileMarkAreaRect(rowRect);
    return QRect(area.left() + (area.width() - ProjectFileMarkSize) / 2,
                 area.top() + (area.height() - ProjectFileMarkSize) / 2,
                 ProjectFileMarkSize,
                 ProjectFileMarkSize);
}

QPainterPath starPath(const QRectF &rect)
{
    constexpr double pi = 3.14159265358979323846;
    const QPointF center = rect.center();
    const double outerRadius = qMin(rect.width(), rect.height()) / 2.0;
    const double innerRadius = outerRadius * 0.48;

    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        const double angle = -pi / 2.0 + i * pi / 5.0;
        const double radius = (i % 2 == 0) ? outerRadius : innerRadius;
        const QPointF point(center.x() + std::cos(angle) * radius,
                            center.y() + std::sin(angle) * radius);
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    path.closeSubpath();
    return path;
}

void drawProjectFileMark(QPainter *painter,
                         const QRect &rowRect,
                         bool marked,
                         bool directlyMarked)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF iconRect =
        QRectF(projectFileMarkIconRect(rowRect)).adjusted(1.5, 1.5, -1.5, -1.5);
    const QPainterPath path = starPath(iconRect);
    if (directlyMarked) {
        painter->setPen(QPen(QColor(170, 110, 0), 1.1));
        painter->setBrush(QColor(255, 183, 36));
    } else if (marked) {
        QColor inheritedFill(255, 183, 36);
        inheritedFill.setAlphaF(0.30);
        QColor inheritedStroke(170, 110, 0);
        inheritedStroke.setAlphaF(0.55);
        painter->setPen(QPen(inheritedStroke, 1.1));
        painter->setBrush(inheritedFill);
    } else {
        painter->setPen(QPen(QColor(174, 181, 190), 1.15));
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawPath(path);
    painter->restore();
}

QIcon emptyProjectFileMarkIcon()
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPainterPath path =
        starPath(QRectF(2.5, 2.5, 13.0, 13.0));
    painter.setPen(QPen(QColor(174, 181, 190), 1.3));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    return QIcon(pixmap);
}

void drawProjectExplorerRowGuide(QPainter *painter,
                                 const QRect &rowRect,
                                 bool selected)
{
    if (selected)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(QPen(QColor(232, 237, 242), 1));
    painter->drawLine(rowRect.left() + 2,
                      rowRect.bottom(),
                      rowRect.right() - 2,
                      rowRect.bottom());
    painter->restore();
}

void drawProjectFileDetail(QPainter *painter,
                           const QRect &rowRect,
                           const QString &text,
                           bool selected)
{
    if (text.isEmpty())
        return;

    const QRect textRect(
        rowRect.right() - ProjectFileMarkAreaWidth
            - ProjectFileDetailAreaWidth + 1,
        rowRect.top(),
        ProjectFileDetailAreaWidth - 8,
        rowRect.height());

    painter->save();
    QFont font = painter->font();
    if (font.pointSize() > 0)
        font.setPointSize(qMax(1, font.pointSize() - 1));
    painter->setFont(font);
    painter->setPen(selected ? QColor("#ffffff") : QColor("#6b7280"));
    painter->drawText(textRect,
                      Qt::AlignRight | Qt::AlignVCenter,
                      text);
    painter->restore();
}

class ProjectExplorerDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (index.data(ProjectExplorerGroupRole).toBool()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        if (index.column() != 0) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const QString imageDimensions =
            index.data(ProjectFileImageDimensionsRole).toString();
        const QString pointCount =
            index.data(ProjectFilePointCountRole).toString();
        const QString detail = imageDimensions.isEmpty()
            ? pointCount
            : imageDimensions;
        const bool showDetail = !detail.isEmpty()
            && option.rect.width() >= ProjectFileDetailMinimumRowWidth;

        QStyleOptionViewItem contentOption = option;
        contentOption.rect.setRight(
            option.rect.right() - ProjectFileMarkAreaWidth
            - (showDetail ? ProjectFileDetailAreaWidth : 0));
        if (option.state.testFlag(QStyle::State_Selected)) {
            painter->fillRect(option.rect, option.palette.highlight());
        }
        QStyledItemDelegate::paint(painter, contentOption, index);
        if (showDetail) {
            drawProjectFileDetail(
                painter,
                option.rect,
                detail,
                option.state.testFlag(QStyle::State_Selected));
        }
        drawProjectExplorerRowGuide(
            painter,
            option.rect,
            option.state.testFlag(QStyle::State_Selected));
        drawProjectFileMark(
            painter,
            option.rect,
            index.data(ProjectFileMarkedRole).toBool(),
            index.data(ProjectFileDirectlyMarkedRole).toBool());
    }
};

QString normalizedArtifactPath(const QString &path)
{
    if (path.isEmpty())
        return {};

    QString normalized = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString pointCsvContractMessage(const QString &errorMessage)
{
    return QStringLiteral(
        "This file is not a valid point CSV for rcaiman-projects.\n\n"
        "Point CSV contract:\n"
        "- The file must be a .csv file.\n"
        "- The first two columns must be row,col.\n"
        "- Quoted headers such as \"row\",\"col\" are accepted.\n"
        "- row and col values must be numeric.\n"
        "- Extra columns are allowed and are loaded as attributes.\n"
        "- Empty CSV files are allowed if they contain a valid header.\n\n"
        "Parser error:\n%1").arg(errorMessage);
}

struct DroppedArtifactBatch
{
    QStringList tiffPaths;
    QStringList csvPaths;
    QStringList unsupportedPaths;
};

bool isTiffSuffix(const QString &suffix)
{
    return suffix == "tif" || suffix == "tiff";
}

bool isCsvSuffix(const QString &suffix)
{
    return suffix == "csv";
}

DroppedArtifactBatch classifyDroppedArtifacts(const QList<QUrl> &urls)
{
    DroppedArtifactBatch batch;
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty()) {
            batch.unsupportedPaths.append(url.toString());
            continue;
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        if (isTiffSuffix(suffix))
            batch.tiffPaths.append(path);
        else if (isCsvSuffix(suffix))
            batch.csvPaths.append(path);
        else
            batch.unsupportedPaths.append(path);
    }
    return batch;
}

bool sameProjectRoot(const QString &left, const QString &right)
{
    return !left.isEmpty()
        && !right.isEmpty()
        && normalizedArtifactPath(left) == normalizedArtifactPath(right);
}

QJsonObject imageStyleToJson(const ImageLayerStyle &style)
{
    const RasterRenderStyle &render = style.render;
    QJsonObject object;
    object["opacity"] = style.opacity;
    object["blend_mode"] = static_cast<int>(style.blendMode);
    object["mode"] = static_cast<int>(render.mode);
    object["ramp"] = static_cast<int>(render.colorRamp);
    object["stretch"] = static_cast<int>(render.stretchMode);
    object["tail_trim"] = render.tailTrim;
    object["trim_left_tail"] = render.trimLeftTail;
    object["trim_right_tail"] = render.trimRightTail;
    object["trim_percent"] = render.trimPercent;
    object["manual_range_enabled"] = render.manualRangeEnabled;
    object["manual_min"] = render.manualMin;
    object["manual_max"] = render.manualMax;
    object["gamma"] = render.gamma;
    object["band"] = render.band;
    object["red_band"] = render.redBand;
    object["green_band"] = render.greenBand;
    object["blue_band"] = render.blueBand;
    return object;
}

ImageLayerStyle imageStyleFromJson(const QJsonObject &object,
                                   const ImageLayerStyle &fallback)
{
    ImageLayerStyle style = fallback;
    style.opacity = object.value("opacity").toDouble(style.opacity);
    style.blendMode = static_cast<ImageBlendMode>(
        object.value("blend_mode").toInt(static_cast<int>(style.blendMode)));
    RasterRenderStyle &render = style.render;
    render.mode = static_cast<RasterRenderMode>(
        object.value("mode").toInt(static_cast<int>(render.mode)));
    render.colorRamp = static_cast<ColorRamp>(
        object.value("ramp").toInt(static_cast<int>(render.colorRamp)));
    render.stretchMode = static_cast<StretchMode>(
        object.value("stretch").toInt(static_cast<int>(render.stretchMode)));
    render.tailTrim = object.value("tail_trim").toBool(render.tailTrim);
    render.trimLeftTail =
        object.value("trim_left_tail").toBool(render.trimLeftTail);
    render.trimRightTail =
        object.value("trim_right_tail").toBool(render.trimRightTail);
    render.trimPercent =
        object.value("trim_percent").toDouble(render.trimPercent);
    render.manualRangeEnabled =
        object.value("manual_range_enabled").toBool(render.manualRangeEnabled);
    render.manualMin =
        object.value("manual_min").toDouble(render.manualMin);
    render.manualMax =
        object.value("manual_max").toDouble(render.manualMax);
    render.gamma = object.value("gamma").toDouble(render.gamma);
    render.band = object.value("band").toInt(render.band);
    render.redBand = object.value("red_band").toInt(render.redBand);
    render.greenBand = object.value("green_band").toInt(render.greenBand);
    render.blueBand = object.value("blue_band").toInt(render.blueBand);
    return style;
}

QJsonObject pointStyleToJson(const PointLayerStyle &style)
{
    QJsonObject object;
    object["color"] = style.color.name(QColor::HexArgb);
    object["size"] = style.size;
    object["symbol"] = static_cast<int>(style.symbol);
    object["label_field"] = style.labelField;
    object["label_size"] = style.labelSize;
    return object;
}

PointLayerStyle pointStyleFromJson(const QJsonObject &object,
                                   const PointLayerStyle &fallback)
{
    PointLayerStyle style = fallback;
    const QColor color(object.value("color").toString());
    if (color.isValid())
        style.color = color;
    style.size = object.value("size").toDouble(style.size);
    style.symbol = static_cast<PointSymbol>(
        object.value("symbol").toInt(static_cast<int>(style.symbol)));
    style.labelField =
        object.value("label_field").toString(style.labelField);
    style.labelSize =
        object.value("label_size").toDouble(style.labelSize);
    return style;
}

bool pathBelongsToProject(const QString &projectRoot, const QString &path)
{
    const QString relative =
        QDir::fromNativeSeparators(
            QDir(projectRoot).relativeFilePath(
                QFileInfo(path).absoluteFilePath()));
    return relative != ".." && !relative.startsWith("../");
}

bool samePath(const QString &left, const QString &right)
{
    const QString leftPath =
        QDir::cleanPath(QFileInfo(left).absoluteFilePath());
    const QString rightPath =
        QDir::cleanPath(QFileInfo(right).absoluteFilePath());
#ifdef Q_OS_WIN
    return leftPath.compare(rightPath, Qt::CaseInsensitive) == 0;
#else
    return leftPath == rightPath;
#endif
}

QStringList exampleProjectRoots()
{
    QStringList roots;
    roots << QDir(QCoreApplication::applicationDirPath()).filePath("examples");
#ifdef EXAMPLES_DIR
    roots << QString::fromUtf8(EXAMPLES_DIR);
#endif
    roots.removeDuplicates();
    return roots;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QVector<ExampleProjectInfo> discoverExampleProjects()
{
    QVector<ExampleProjectInfo> examples;
    for (const QString &rootPath : exampleProjectRoots()) {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList entries =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                               QDir::Name);
        for (const QFileInfo &entry : entries) {
            QString errorMessage;
            const Project project = Project::open(entry.absoluteFilePath(),
                                                  &errorMessage);
            if (!project.isValid())
                continue;

            ExampleProjectInfo info;
            info.path = project.rootPath();
            info.name = project.name();
            info.description = project.description();
            const QDir projectDir(project.rootPath());
            info.guide = readTextFile(projectDir.filePath("guidelines.md"));
            if (info.guide.trimmed().isEmpty())
                info.guide = readTextFile(projectDir.filePath("README.md"));
            examples.append(info);
        }
    }
    return examples;
}

QPixmap circularThumbnail(const QString &resourcePath, int size)
{
    const QPixmap original(resourcePath);
    if (original.isNull())
        return {};

    QPixmap scaled =
        original.scaled(size, size,
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation);
    const QRect cropRect(
        (scaled.width() - size) / 2,
        (scaled.height() - size) / 2,
        size,
        size);
    scaled = scaled.copy(cropRect);

    QPixmap thumbnail(size, size);
    thumbnail.fill(Qt::transparent);

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clipPath;
    clipPath.addEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));
    painter.setClipPath(clipPath);
    painter.drawPixmap(0, 0, scaled);
    painter.setClipping(false);
    painter.setPen(QPen(QColor(220, 220, 220), 1.0));
    painter.drawEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));

    return thumbnail;
}

QString collectionStyleKind(const Collection &collection)
{
    if (collection.collectionClass == "point_collection")
        return QStringLiteral("point");
    if (collection.collectionClass == "preprocessed_image_collection"
        || collection.collectionClass == "processed_image_collection"
        || collection.collectionClass == "segmentation_collection") {
        return QStringLiteral("image");
    }
    return {};
}

bool collectionStyleMatchesKind(const CollectionStyleData &style,
                                const QString &kind)
{
    if (kind == "point")
        return style.kind == CollectionStyleData::Kind::Point;
    if (kind == "image")
        return style.kind == CollectionStyleData::Kind::Image;
    return false;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectFileSystemModel(new ProjectFileSystemModel(this))
    , m_projectSortProxy(new ProjectExplorerSortProxyModel(this))
    , m_artifactWatcher(new QFileSystemWatcher(this))
{
    ui->setupUi(this);
    statusBar()->setFixedHeight(30);
    setWindowIcon(QIcon(":/branding/rcaiman-icon.png"));
    qApp->setWindowIcon(windowIcon());
    ui->actionPan->setToolTip(ShortcutText::panTooltip());
    ui->actionZoom->setToolTip(ShortcutText::zoomTooltip());
    ui->actionEditPoints->setToolTip(ShortcutText::editPointsTooltip());

    connect(m_artifactWatcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
                scheduleArtifactReload(path);
            });

    m_projectFileSystemModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_projectSortProxy->setSourceModel(m_projectFileSystemModel);
    ui->projectTree->setModel(m_projectSortProxy);
    ui->projectTree->setItemDelegate(
        new ProjectExplorerDelegate(ui->projectTree));
    const QModelIndex fileSystemRoot = projectProxyIndex(
        m_projectFileSystemModel->setRootPath(QString()));
    ui->projectTree->setRootIndex(fileSystemRoot);
    ui->projectTree->setDragEnabled(true);
    ui->projectTree->setAcceptDrops(true);
    ui->projectTree->viewport()->setAcceptDrops(true);
    ui->projectTree->viewport()->installEventFilter(this);
    ui->projectTree->setDropIndicatorShown(true);
    setAcceptDrops(true);
    ui->projectTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->projectTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->projectTree->setHeaderHidden(true);
    ui->projectTree->hideColumn(1);
    ui->projectTree->hideColumn(2);
    ui->projectTree->hideColumn(3);
    ui->projectTree->setContextMenuPolicy(Qt::CustomContextMenu);

    m_newFolderButton = new QToolButton(this);
    m_newFolderButton->setText("New folder");
    m_newFolderButton->setIcon(
        style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_newFolderButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_newFolderButton->setAutoRaise(true);
    m_newFolderButton->setEnabled(false);
    m_unstarAllButton = new QToolButton(this);
    m_unstarAllButton->setText("Unstar all");
    m_unstarAllButton->setIcon(emptyProjectFileMarkIcon());
    m_unstarAllButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_unstarAllButton->setAutoRaise(true);
    m_unstarAllButton->setToolTip("Remove all stars from this project.");
    m_unstarAllButton->setEnabled(false);
    m_starredOnlyCheck = new QCheckBox("Starred only", this);
    m_starredOnlyCheck->setToolTip(
        "Show starred files and folders. Starred folders include their contents.");
    m_projectSortCombo = new QComboBox(this);
    m_projectSortCombo->addItem("Order files by name", static_cast<int>(ProjectExplorerSortMode::Name));
    m_projectSortCombo->addItem("Order files by type", static_cast<int>(ProjectExplorerSortMode::Type));
    m_projectSortCombo->setToolTip("Sort Project Explorer");
    m_projectSortCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_projectSortCombo->setMinimumWidth(
        m_projectSortCombo->fontMetrics().horizontalAdvance(
            QStringLiteral("Order files by name")) + 22);
    const int savedSortMode =
        AppPreferences::projectExplorerSortMode(
            static_cast<int>(ProjectExplorerSortMode::Name));
    const int savedSortIndex = m_projectSortCombo->findData(savedSortMode);
    if (savedSortIndex >= 0)
        m_projectSortCombo->setCurrentIndex(savedSortIndex);
    auto *projectSortProxy =
        static_cast<ProjectExplorerSortProxyModel*>(m_projectSortProxy);
    projectSortProxy->setSortMode(static_cast<ProjectExplorerSortMode>(
        m_projectSortCombo->currentData().toInt()));
    projectSortProxy->setStarredOnly(false);
    m_projectSortProxy->sort(0, Qt::AscendingOrder);

    auto *projectExplorerToolbar = new QWidget(this);
    auto *projectExplorerToolbarLayout =
        new QHBoxLayout(projectExplorerToolbar);
    projectExplorerToolbarLayout->setContentsMargins(0, 0, 0, 0);
    projectExplorerToolbarLayout->setSpacing(4);
    projectExplorerToolbarLayout->addWidget(m_newFolderButton);
    projectExplorerToolbarLayout->addWidget(m_unstarAllButton);
    projectExplorerToolbarLayout->addWidget(m_starredOnlyCheck);
    projectExplorerToolbarLayout->addStretch(1);
    projectExplorerToolbarLayout->addWidget(m_projectSortCombo);

    m_collectionGraphButton = new QPushButton("Collection graph...", this);
    m_collectionGraphButton->setEnabled(false);
    m_collectionGraphButton->setToolTip("Show the registered collection graph.");
    m_checkCollectionsButton = new QPushButton("Check collections...", this);
    m_checkCollectionsButton->setEnabled(false);
    m_checkCollectionsButton->setToolTip(
        "Check registered collections for broken links and invalid fields.");
    auto *projectStructureToolbar = new QWidget(this);
    auto *projectStructureToolbarLayout =
        new QHBoxLayout(projectStructureToolbar);
    projectStructureToolbarLayout->setContentsMargins(0, 0, 0, 0);
    projectStructureToolbarLayout->setSpacing(4);
    projectStructureToolbarLayout->addWidget(m_collectionGraphButton, 1);
    projectStructureToolbarLayout->addWidget(m_checkCollectionsButton, 1);

    m_projectHeaderWidget = new QWidget(this);
    auto *projectHeaderLayout = new QVBoxLayout(m_projectHeaderWidget);
    projectHeaderLayout->setContentsMargins(4, 2, 4, 6);
    projectHeaderLayout->setSpacing(1);
    m_projectDescriptionLabel =
        new ElidedLabel(Qt::ElideRight, m_projectHeaderWidget);
    QFont descriptionFont = m_projectDescriptionLabel->font();
    descriptionFont.setBold(true);
    m_projectDescriptionLabel->setFont(descriptionFont);
    m_projectDescriptionLabel->setStyleSheet(
        "QLabel { color: #334155; }");
    m_projectPathLabel =
        new ElidedLabel(Qt::ElideMiddle, m_projectHeaderWidget);
    m_projectPathLabel->setStyleSheet(
        "QLabel { color: #64748b; font-size: 11px; }");
    projectHeaderLayout->addWidget(m_projectDescriptionLabel);
    projectHeaderLayout->addWidget(m_projectPathLabel);
    m_projectHeaderWidget->setVisible(false);

    ui->dockProjectExplorerLayout->insertWidget(0, m_projectHeaderWidget);
    ui->dockProjectExplorerLayout->insertWidget(1, projectExplorerToolbar);
    ui->dockProjectExplorerLayout->insertWidget(2, projectStructureToolbar);
    m_projectStarredEmptyLabel = new QLabel(
        "No starred files or folders.", this);
    m_projectStarredEmptyLabel->setWordWrap(true);
    m_projectStarredEmptyLabel->setAlignment(Qt::AlignCenter);
    m_projectStarredEmptyLabel->setStyleSheet(
        "QLabel { color: #6f7f90; padding: 8px 4px; }");
    m_projectStarredEmptyLabel->setVisible(false);
    ui->dockProjectExplorerLayout->insertWidget(3, m_projectStarredEmptyLabel);
    connect(m_newFolderButton, &QToolButton::clicked,
            this, &MainWindow::createProjectFolder);
    connect(m_unstarAllButton, &QToolButton::clicked,
            this, &MainWindow::clearProjectFileMarks);
    connect(m_collectionGraphButton, &QPushButton::clicked,
            this, &MainWindow::showCollectionGraph);
    connect(m_checkCollectionsButton, &QPushButton::clicked,
            this, &MainWindow::checkCollections);
    connect(m_starredOnlyCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                auto *proxy = static_cast<ProjectExplorerSortProxyModel*>(
                    m_projectSortProxy);
                proxy->setStarredOnly(checked);
                refreshProjectExplorerRootIndex();
                updateProjectExplorerStarredEmptyMessage();
                saveProjectExplorerState();
                if (checked && m_projectFileMarks.isEmpty()) {
                    statusBar()->showMessage(
                        "No starred files or folders",
                        3000);
                }
            });
    connect(m_projectSortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                auto *proxy = static_cast<ProjectExplorerSortProxyModel*>(
                    m_projectSortProxy);
                proxy->setSortMode(static_cast<ProjectExplorerSortMode>(
                    m_projectSortCombo->currentData().toInt()));
                m_projectSortProxy->sort(0, Qt::AscendingOrder);
                AppPreferences::setProjectExplorerSortMode(
                    m_projectSortCombo->currentData().toInt());
            });

    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsInserted,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QAbstractItemModel::rowsRemoved,
            this, [this]() { updateProjectActions(); });
    connect(m_projectFileSystemModel, &QFileSystemModel::directoryLoaded,
            this, [this]() {
                refreshProjectExplorerRootIndex();
                updateProjectExplorerStarredEmptyMessage();
                updateProjectActions();
            });
    connect(m_projectFileSystemModel, &QFileSystemModel::fileRenamed,
            this, [this](const QString &path, const QString&, const QString&) {
                if (!m_project.isValid() || !samePath(path, m_project.rootPath()))
                    return;

                m_projectFileSystemModel->setProjectRoot(m_project.rootPath());
                static_cast<ProjectExplorerSortProxyModel*>(
                    m_projectSortProxy)->invalidate();
                QTimer::singleShot(
                    0, this, &MainWindow::refreshProjectExplorerRootIndex);
                updateProjectActions();
            });
    connect(m_projectSortProxy, &QAbstractItemModel::modelReset,
            this, [this]() {
                QTimer::singleShot(
                    0, this, &MainWindow::refreshProjectExplorerRootIndex);
            });
    connect(m_projectSortProxy, &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex&, int, int) {
                QTimer::singleShot(
                    0, this, &MainWindow::refreshProjectExplorerRootIndex);
            });
    connect(ui->projectTree, &QTreeView::expanded,
            this, [this](const QModelIndex &index) {
                rememberProjectExplorerGroupExpansion(index, true);
            });
    connect(ui->projectTree, &QTreeView::collapsed,
            this, [this](const QModelIndex &index) {
                rememberProjectExplorerGroupExpansion(index, false);
            });

    // Load recent projects
    m_recentProjects = AppPreferences::recentProjects();

    connect(ui->actionNewProject,
            &QAction::triggered,
            this,
            &MainWindow::newProject);

    connect(ui->actionOpenProject,
            &QAction::triggered,
            this,
            &MainWindow::openProject);

    connect(ui->actionCloseProject,
            &QAction::triggered,
            this,
            &MainWindow::closeProject);

    connect(ui->actionSaveProjectAs,
            &QAction::triggered,
            this,
            &MainWindow::saveProjectAs);

    connect(ui->actionSaveProjectAsCleanCopy,
            &QAction::triggered,
            this,
            &MainWindow::saveProjectAsCleanCopy);

    connect(ui->actionCleanProject,
            &QAction::triggered,
            this,
            &MainWindow::cleanProject);

    connect(ui->actionEditProjectDescription,
            &QAction::triggered,
            this,
            &MainWindow::editProjectDescription);

    connect(ui->actionProjectReport,
            &QAction::triggered,
            this,
            &MainWindow::createProjectReport);

    connect(ui->actionImportLegacyHspProject,
            &QAction::triggered,
            this,
            &MainWindow::importLegacyHspProject);

    connect(ui->actionOpen_Image,
            &QAction::triggered,
            this,
            &MainWindow::openImage);

    connect(ui->actionImportRaw,
            &QAction::triggered,
            this,
            &MainWindow::importRaw);

    connect(ui->actionExtractRawData,
            &QAction::triggered,
            this,
            &MainWindow::extractRawData);

    connect(ui->actionImportLegacyJpegs,
            &QAction::triggered,
            this,
            &MainWindow::importLegacyJpegs);

    connect(ui->actionRegisterCollection,
            &QAction::triggered,
            this,
            &MainWindow::registerCollection);

    connect(ui->actionCheckCollections,
            &QAction::triggered,
            this,
            &MainWindow::checkCollections);

    connect(ui->actionCollectionGraph,
            &QAction::triggered,
            this,
            &MainWindow::showCollectionGraph);

    connect(ui->actionBuildMetadataRules,
            &QAction::triggered,
            this,
            &MainWindow::buildMetadataRules);

    connect(ui->actionValidateImageMetadata,
            &QAction::triggered,
            this,
            &MainWindow::validateImageMetadata);

    connect(ui->actionValidateCollectionMetadata,
            &QAction::triggered,
            this,
            &MainWindow::validateCollectionMetadata);

    connect(ui->actionCreatePoints,
            &QAction::triggered,
            this,
            &MainWindow::createPointsCsv);
    connect(ui->actionCreatePointGrid,
            &QAction::triggered,
            this,
            &MainWindow::createPointGrid);
    connect(ui->actionSelectPointsByMask,
            &QAction::triggered,
            this,
            &MainWindow::selectPointsByMask);
    connect(ui->actionPointFastPreviewLimit,
            &QAction::triggered,
            this,
            &MainWindow::configurePointFastPreviewLimit);

    connect(ui->actionCreateMask,
            &QAction::triggered,
            this,
            &MainWindow::createMaskTiff);

    connect(ui->actionEditMask,
            &QAction::toggled,
            this,
            &MainWindow::onEditMaskToggled);

    connect(ui->actionInvertMask,
            &QAction::triggered,
            this,
            &MainWindow::onInvertMaskTriggered);

    connect(ui->actionCancelMaskEdit,
            &QAction::triggered,
            this,
            &MainWindow::onCancelMaskEditTriggered);

    connect(ui->actionPan,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                if (enabled && ui->actionZoom->isChecked())
                    ui->actionZoom->setChecked(false);
                ImageViewer *viewer = currentImageViewer();
                if (viewer)
                    viewer->setPanModeEnabled(enabled);
            });

    connect(ui->actionZoom,
            &QAction::toggled,
            this,
            [this](bool enabled) {
                if (enabled && ui->actionPan->isChecked())
                    ui->actionPan->setChecked(false);
                ImageViewer *viewer = currentImageViewer();
                if (viewer)
                    viewer->setZoomModeEnabled(enabled);
            });

    m_zoomPercentSpin = new QDoubleSpinBox(this);
    m_zoomPercentSpin->setRange(1.0, 6400.0);
    m_zoomPercentSpin->setDecimals(0);
    m_zoomPercentSpin->setSingleStep(10.0);
    m_zoomPercentSpin->setSuffix("%");
    m_zoomPercentSpin->setToolTip(
        "Viewer zoom level. Enter 100 for actual size; press 0 in the viewer for 100%.");
    m_zoomPercentSpin->setEnabled(false);
    m_zoomPercentSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_zoomPercentSpin->setMinimumWidth(
        m_zoomPercentSpin->fontMetrics().horizontalAdvance(
            QStringLiteral("6400%")) + 18);
    ui->toolBar->insertWidget(ui->actionSplitViewer, m_zoomPercentSpin);
    connect(m_zoomPercentSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,
            [this](double value) {
                ImageViewer *viewer = currentImageViewer();
                if (!viewer)
                    return;
                if (!qFuzzyCompare(viewer->zoomPercent() + 1.0, value + 1.0))
                    viewer->setZoomPercent(value);
            });

    connect(ui->actionEditPoints,
            &QAction::toggled,
            this,
            &MainWindow::onEditPointsToggled);

    connect(ui->actionCancelEdit,
            &QAction::triggered,
            this,
            &MainWindow::onCancelEditTriggered);

    m_maskBrushSlider = new QSlider(Qt::Horizontal, this);
    m_maskBrushSlider->setRange(0, MaskBrushSliderMaximum);
    m_maskBrushSlider->setValue(maskBrushSliderFromSize(9, 200));
    m_maskBrushSlider->setToolTip(
        QStringLiteral(
            "Mask brush size in raster pixels.\n"
            "Use [ and ] in the viewer to adjust.\n"
            "Shift+click draws a straight brush stroke.\n"
            "%1+double click inverts a connected binary region.")
            .arg(ShortcutText::primaryModifier()));
    m_maskBrushSlider->setEnabled(false);
    m_maskBrushSlider->setFixedWidth(96);
    ui->toolBar->addWidget(m_maskBrushSlider);

    m_maskBrushSpin = new QSpinBox(this);
    m_maskBrushSpin->setRange(1, 200);
    m_maskBrushSpin->setValue(9);
    m_maskBrushSpin->setSuffix(" px");
    m_maskBrushSpin->setToolTip(
        QStringLiteral(
            "Mask brush size in raster pixels.\n"
            "Use [ and ] in the viewer to adjust.\n"
            "Shift+click draws a straight brush stroke.\n"
            "%1+double click inverts a connected binary region.")
            .arg(ShortcutText::primaryModifier()));
    m_maskBrushSpin->setEnabled(false);
    m_maskBrushSpin->setMinimumWidth(
        m_maskBrushSpin->fontMetrics().horizontalAdvance(
            QStringLiteral("200 px")) + 32);
    ui->toolBar->addWidget(m_maskBrushSpin);
    connect(m_maskBrushSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (ImageViewer *viewer = currentImageViewer())
                    viewer->setMaskBrushSize(value);
            });
    connect(m_maskBrushSlider,
            &QSlider::valueChanged,
            this,
            [this](int sliderValue) {
                ImageViewer *viewer = currentImageViewer();
                if (!viewer || !m_maskBrushSpin)
                    return;
                const int size = maskBrushSizeFromSlider(
                    sliderValue,
                    viewer->maskBrushMaximum());
                viewer->setMaskBrushSize(size);
            });

    connect(ui->actionSplitViewer,
            &QAction::triggered,
            this,
            &MainWindow::splitViewer);

    connect(ui->actionSyncSplitViews,
            &QAction::toggled,
            this,
            &MainWindow::onSyncSplitViewsToggled);

    connect(ui->actionSyncSplitZoom,
            &QAction::toggled,
            this,
            &MainWindow::onSyncSplitZoomToggled);

    connect(ui->actionResetWindowLayout,
            &QAction::triggered,
            this,
            &MainWindow::resetWindowLayout);
    connect(ui->actionAboutRcaimanProjects,
            &QAction::triggered,
            this,
            &MainWindow::showAboutDialog);
    connect(ui->actionInteractionShortcuts,
            &QAction::triggered,
            this,
            &MainWindow::showInteractionShortcutsDialog);
    connect(ui->actionExampleProjects,
            &QAction::triggered,
            this,
            &MainWindow::createProjectFromExample);

    connect(ui->projectTree,
            &QTreeView::doubleClicked,
            this,
            &MainWindow::onProjectTreeDoubleClicked);

    connect(ui->projectTree,
            &QTreeView::clicked,
            this,
            &MainWindow::onProjectTreeItemClicked);
    connect(ui->projectTree->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex &current, const QModelIndex &) {
                onProjectTreeItemClicked(current);
            });

    connect(ui->projectTree,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::onProjectTreeContextMenu);

    connect(ui->workspaceTabs,
            &QTabWidget::tabCloseRequested,
            this,
            &MainWindow::onWorkspaceTabCloseRequested);

    connect(ui->workspaceTabs,
            &QTabWidget::currentChanged,
            this,
            &MainWindow::onWorkspaceTabChanged);

    ui->workspaceTabs->setTabsClosable(true);
    ui->workspaceTabs->setElideMode(Qt::ElideMiddle);
    ui->workspaceTabs->setMovable(true);
    ui->workspaceTabs->setDocumentMode(true);
    ui->workspaceTabs->setAcceptDrops(true);
    ui->workspaceTabs->installEventFilter(this);
    if (ui->workspaceTabs->tabBar()) {
        ui->workspaceTabs->tabBar()->setAcceptDrops(true);
        ui->workspaceTabs->tabBar()->installEventFilter(this);
        ui->workspaceTabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->workspaceTabs->tabBar(),
                &QWidget::customContextMenuRequested,
                this,
                &MainWindow::onWorkspaceTabContextMenu);
    }
    createViewerEmptyState();

    // Obtain LayersDock from the UI instead of creating it again
    if (QDockWidget *dockLayers = findChild<QDockWidget *>("dockLayers")) {
        m_layersDock = dockLayers->findChild<LayersDock *>("layersDock");
    } else {
        m_layersDock = new LayersDock(this);
        auto *fallbackDockLayers = new QDockWidget("Layers", this);
        fallbackDockLayers->setObjectName("dockLayers");
        fallbackDockLayers->setWidget(m_layersDock);
        addDockWidget(Qt::RightDockWidgetArea, fallbackDockLayers);
    }
    if (m_layersDock && m_project.isValid())
        m_layersDock->setProjectPath(m_project.rootPath());

    if (m_layersDock) {
        connect(m_layersDock, &LayersDock::activeLayerChanged,
                this, &MainWindow::onLayerSelectionChanged);
        connect(m_layersDock, &LayersDock::pointAttributeTableRequested,
                this, &MainWindow::openAttributeTable);
    }

    // Ensure 3-column layout: Explorer (left), Viewer (center), Layers+Properties (right)
    setDockNestingEnabled(true);
    addDockWidget(Qt::LeftDockWidgetArea, ui->dockProjectExplorer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockViewer);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockLayers);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockProperties);

    // Split into 3 areas: Explorer | Viewer | (Layers ⊓ Properties)
    splitDockWidget(ui->dockViewer, ui->dockLayers, Qt::Horizontal);
    splitDockWidget(ui->dockLayers, ui->dockProperties, Qt::Vertical);

    // Preview Dock: below Project Explorer
    m_previewDock = new PreviewDock(this);
    m_previewDock->setObjectName("dockPreview");
    addDockWidget(Qt::LeftDockWidgetArea, m_previewDock);
    splitDockWidget(ui->dockProjectExplorer, m_previewDock, Qt::Vertical);
    m_previewDock->hide(); // hidden until a project is loaded

    m_histogramDock = new HistogramDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_histogramDock);
    tabifyDockWidget(ui->dockProperties, m_histogramDock);
    m_histogramDock->raise();
    connect(m_histogramDock, &HistogramDock::computationStarted,
            this, [this](const QString &message) {
                beginStatusProgress(message);
            });
    connect(m_histogramDock, &HistogramDock::computationFinished,
            this, [this](const QString &message) {
                endStatusProgress(message, 1500);
            });

    configureDockSizing();

    // Initial dock proportions.
    resizeDocks({ui->dockProjectExplorer, ui->dockViewer, ui->dockLayers},
                {420, 600, 260}, Qt::Horizontal);

    connect(ui->dockProperties, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    ui->dockProperties->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = ui->dockProperties->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    ui->dockProperties->setMinimumWidth(220);
                    ui->dockProperties->setMaximumWidth(320);
                    if (QWidget *contents = ui->dockProperties->widget()) {
                        contents->setMinimumWidth(220);
                        contents->setMaximumWidth(320);
                    }
                }
            });
    connect(m_previewDock, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    m_previewDock->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = m_previewDock->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    m_previewDock->setMinimumWidth(260);
                    m_previewDock->setMaximumWidth(460);
                    if (QWidget *contents = m_previewDock->widget()) {
                        contents->setMinimumWidth(260);
                        contents->setMaximumWidth(460);
                    }
                }
            });
    connect(m_histogramDock, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                if (floating) {
                    m_histogramDock->setMaximumWidth(QWIDGETSIZE_MAX);
                    if (QWidget *contents = m_histogramDock->widget())
                        contents->setMaximumWidth(QWIDGETSIZE_MAX);
                } else {
                    m_histogramDock->setMinimumWidth(220);
                    m_histogramDock->setMaximumWidth(320);
                    if (QWidget *contents = m_histogramDock->widget()) {
                        contents->setMinimumWidth(220);
                        contents->setMaximumWidth(320);
                    }
                }
            });

    m_defaultWindowState = saveState(1);
    updateProjectActions();
    updateViewerEmptyState();
    updateRecentProjectsMenu();
    restoreApplicationState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createViewerEmptyState()
{
    if (m_viewerEmptyState)
        return;

    m_viewerEmptyState = new QWidget(ui->dockViewerContents);
    m_viewerEmptyState->setAcceptDrops(true);
    m_viewerEmptyState->installEventFilter(this);
    m_viewerEmptyState->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *outerLayout = new QVBoxLayout(m_viewerEmptyState);
    outerLayout->setContentsMargins(48, 48, 48, 48);
    outerLayout->addStretch(1);

    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(10);
    contentLayout->setAlignment(Qt::AlignCenter);

    m_viewerEmptyTitle = new QLabel(m_viewerEmptyState);
    QFont titleFont = m_viewerEmptyTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    m_viewerEmptyTitle->setFont(titleFont);
    m_viewerEmptyTitle->setAlignment(Qt::AlignCenter);
    m_viewerEmptyTitle->setStyleSheet("color: #6f7782;");
    m_viewerEmptyTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerEmptyTitle);

    auto *rule = new QFrame(m_viewerEmptyState);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Plain);
    rule->setFixedWidth(360);
    rule->setStyleSheet("color: #9aa0a6;");
    rule->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(rule, 0, Qt::AlignCenter);

    m_viewerEmptyBody = new QLabel(m_viewerEmptyState);
    m_viewerEmptyBody->setWordWrap(true);
    m_viewerEmptyBody->setTextFormat(Qt::RichText);
    m_viewerEmptyBody->setAlignment(Qt::AlignLeft);
    m_viewerEmptyBody->setMaximumWidth(520);
    m_viewerEmptyBody->setStyleSheet("color: #7b8490;");
    m_viewerEmptyBody->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerEmptyBody);

    contentLayout->addSpacing(22);

    m_viewerQuickTitle = new QLabel(m_viewerEmptyState);
    m_viewerQuickTitle->setFont(titleFont);
    m_viewerQuickTitle->setAlignment(Qt::AlignCenter);
    m_viewerQuickTitle->setStyleSheet("color: #6f7782;");
    m_viewerQuickTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickTitle);

    m_viewerQuickRule = new QFrame(m_viewerEmptyState);
    m_viewerQuickRule->setFrameShape(QFrame::HLine);
    m_viewerQuickRule->setFrameShadow(QFrame::Plain);
    m_viewerQuickRule->setFixedWidth(360);
    m_viewerQuickRule->setStyleSheet("color: #9aa0a6;");
    m_viewerQuickRule->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickRule, 0, Qt::AlignCenter);

    m_viewerQuickBody = new QLabel(m_viewerEmptyState);
    m_viewerQuickBody->setWordWrap(true);
    m_viewerQuickBody->setTextFormat(Qt::RichText);
    m_viewerQuickBody->setAlignment(Qt::AlignLeft);
    m_viewerQuickBody->setMaximumWidth(520);
    m_viewerQuickBody->setStyleSheet("color: #7b8490;");
    m_viewerQuickBody->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(m_viewerQuickBody);

    outerLayout->addLayout(contentLayout);
    outerLayout->addStretch(2);

    ui->dockViewerLayout->addWidget(m_viewerEmptyState);
}

void MainWindow::updateViewerEmptyState()
{
    if (!m_viewerEmptyState || !ui->workspaceTabs)
        return;

    const bool isEmpty = ui->workspaceTabs->count() == 0;
    ui->workspaceTabs->setVisible(!isEmpty);
    m_viewerEmptyState->setVisible(isEmpty);
    if (!isEmpty)
        return;

    if (m_project.isValid()) {
        m_viewerEmptyTitle->setText("Open a TIFF");
        m_viewerEmptyBody->setText(
            "<ul>"
            "<li>Double-click a TIFF in Project Explorer.</li>"
            "<li>Drag and drop a TIFF here.</li>"
            "<li>Use <b>Image &gt; Open...</b>.</li>"
            "</ul>"
            "<p>Point CSV layers can be added after an image viewer is open.</p>");
        m_viewerQuickTitle->hide();
        m_viewerQuickRule->hide();
        m_viewerQuickBody->hide();
    } else {
        m_viewerEmptyTitle->setText("Open or create a project");
        m_viewerEmptyBody->setText(
            "<ul>"
            "<li><b>Project &gt; New...</b></li>"
            "<li><b>Project &gt; Open...</b></li>"
            "<li><b>Help &gt; Example projects...</b></li>"
            "</ul>");
        m_viewerQuickTitle->setText("Quick view without creating a project");
        m_viewerQuickBody->setText(
            "<ul>"
            "<li>Drag and drop a TIFF here to inspect it.</li>"
            "<li>Drag and drop a compatible point CSV after a TIFF is open.</li>"
            "</ul>");
        m_viewerQuickTitle->show();
        m_viewerQuickRule->show();
        m_viewerQuickBody->show();
    }
}

void MainWindow::showAboutDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("About rcaiman-projects");
    dialog.setWindowIcon(QIcon(":/branding/rcaiman-icon.png"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *brandSceneLayout = new QHBoxLayout();
    brandSceneLayout->setSpacing(14);
    brandSceneLayout->addStretch();
    const QStringList hemisphericImages = {
        ":/branding/hemispheric-1.jpg",
        ":/branding/hemispheric-2.jpg",
        ":/branding/hemispheric-3.jpg"
    };
    for (const QString &imagePath : hemisphericImages) {
        const QPixmap thumbnail = circularThumbnail(imagePath, 56);
        if (thumbnail.isNull())
            continue;
        auto *imageLabel = new QLabel(&dialog);
        imageLabel->setFixedSize(56, 56);
        imageLabel->setPixmap(thumbnail);
        brandSceneLayout->addWidget(imageLabel);
    }

    QPixmap iconPixmap(":/branding/rcaiman-icon.png");
    if (!iconPixmap.isNull()) {
        auto *iconLabel = new QLabel(&dialog);
        iconLabel->setFixedSize(88, 88);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(
            iconPixmap.scaled(88, 88,
                              Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
        brandSceneLayout->addWidget(iconLabel);
    }
    brandSceneLayout->addStretch();
    layout->addLayout(brandSceneLayout);

    QPixmap logoPixmap(":/branding/rcaiman-projects-logo.png");
    if (!logoPixmap.isNull()) {
        auto *logoLabel = new QLabel(&dialog);
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setPixmap(
            logoPixmap.scaledToWidth(420, Qt::SmoothTransformation));
        layout->addWidget(logoLabel);
    }

    QString aboutText;
    QFile aboutFile(":/branding/about.md");
    if (aboutFile.open(QIODevice::ReadOnly | QIODevice::Text))
        aboutText = QString::fromUtf8(aboutFile.readAll());
    if (aboutText.trimmed().isEmpty())
        aboutText = "rcaiman-projects";

    auto *textBrowser = new QTextBrowser(&dialog);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setMarkdown(aboutText);
    textBrowser->setMinimumSize(460, 180);
    layout->addWidget(textBrowser);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::showInteractionShortcutsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Interaction shortcuts");
    dialog.setWindowIcon(windowIcon());

    auto *layout = new QVBoxLayout(&dialog);

    auto *textBrowser = new QTextBrowser(&dialog);
    textBrowser->setMarkdown(ShortcutText::interactionShortcutsMarkdown());
    textBrowser->setMinimumSize(520, 420);
    layout->addWidget(textBrowser);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::createProjectFromExample()
{
    const QVector<ExampleProjectInfo> examples = discoverExampleProjects();
    ExampleProjectsDialog dialog(examples, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExampleProjectInfo example = dialog.selectedExample();
    if (example.path.isEmpty())
        return;

    const QString parentPath = QFileDialog::getExistingDirectory(
        this, "Select Destination Parent Folder");
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Create Example Project",
        "New project name:",
        QLineEdit::Normal,
        example.name,
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project project = Project::copyExistingProject(
        example.path, destination, projectName, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Example Project", errorMessage);
        return;
    }

    loadProject(project);
    statusBar()->showMessage(
        QStringLiteral("Example project created: %1").arg(destination),
        5000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveProjectSession();
    saveApplicationState();
    QMainWindow::closeEvent(event);
}

void MainWindow::addToRecentProjects(const QString &path)
{
    m_recentProjects.removeAll(path);
    m_recentProjects.push_front(path);
    if (m_recentProjects.size() > 5)
        m_recentProjects.erase(m_recentProjects.end() - 1, m_recentProjects.end());
    AppPreferences::setRecentProjects(m_recentProjects);
    updateRecentProjectsMenu();
}

void MainWindow::updateRecentProjectsMenu()
{
    QMenu *recentMenu = findChild<QMenu *>("menuRecentProjects");
    if (!recentMenu)
        return;
    qDeleteAll(recentMenu->actions());
    recentMenu->clear();

    bool hasRecent = false;
    for (const QString &path : std::as_const(m_recentProjects)) {
        if (path.isEmpty())
            continue;
        hasRecent = true;
        const QFileInfo projectInfo(path);
        QAction *act = recentMenu->addAction(
            QStringLiteral("%1 - %2")
                .arg(projectInfo.fileName(),
                     projectInfo.dir().dirName()));
        act->setToolTip(path);
        connect(act, &QAction::triggered, this, [this, path]() {
            QString errorMessage;
            const Project project = Project::open(path, &errorMessage);
            if (!project.isValid()) {
                QMessageBox::warning(this, "Open Recent Project", errorMessage);
                return;
            }
            loadProject(project);
        });
    }
    if (!hasRecent) {
        auto *emptyAct = recentMenu->addAction("(empty)");
        emptyAct->setEnabled(false);
    }
}

void MainWindow::newProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString rootPath =
        QDir(dialog.parentDirectory()).filePath(dialog.folderName());
    QString errorMessage;
    const Project project = Project::create(
        rootPath, dialog.folderName(), dialog.description(), &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "New Project", errorMessage);
        return;
    }

    loadProject(project);
}

void MainWindow::openProject()
{
    const QString rootPath = QFileDialog::getExistingDirectory(
        this, "Open Project Folder");
    if (rootPath.isEmpty())
        return;

    QString errorMessage;
    const Project project = Project::open(rootPath, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Open Project", errorMessage);
        return;
    }

    loadProject(project);
}

void MainWindow::importLegacyHspProject()
{
    HspImportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    HspImportOptions options;
    options.sourceProjectPath = dialog.sourceDirectory();
    options.destinationProjectPath =
        QDir(dialog.parentDirectory()).filePath(dialog.folderName());
    options.projectName = dialog.folderName();
    options.description = dialog.description();

    HspImportReport report;
    QString errorMessage;
    const Project project =
        HspProjectImporter::importProject(options, &report, &errorMessage);
    if (!project.isValid()) {
        QMessageBox::warning(this, "Import legacy HSP project", errorMessage);
        return;
    }

    loadProject(project);

    QString message = QStringLiteral(
        "Imported legacy HSP project.\n\n"
        "RAW files copied: %1\n"
        "PGM images converted: %2\n"
        "Sky point CSV files: %3\n"
        "Sun mark CSV files: %4\n"
        "HSP mask TIFF files: %5\n"
        "HSP marked area TIFF files: %6\n"
        "Optimized sky coefficient TSV files: %7")
        .arg(report.rawFilesCopied)
        .arg(report.pgmFilesConverted)
        .arg(report.skyPointFilesWritten)
        .arg(report.sunMarkFilesWritten)
        .arg(report.maskFilesWritten)
        .arg(report.markedAreaFilesWritten)
        .arg(report.optSkyCoefTablesWritten);
    if (!report.warnings.isEmpty()) {
        message += QStringLiteral(
            "\n\nWarnings were written to legacy_hsp_import.md.");
    }
    QMessageBox::information(this, "Import legacy HSP project", message);
    statusBar()->showMessage(
        QStringLiteral("Imported legacy HSP project: %1")
            .arg(project.rootPath()),
        5000);
}

void MainWindow::closeProject()
{
    if (!m_project.isValid())
        return;

    closeCurrentProject();
    statusBar()->showMessage("Project closed", 3000);
}

void MainWindow::saveProjectAs()
{
    if (!m_project.isValid())
        return;

    saveProjectSession();

    const QString parentPath = QFileDialog::getExistingDirectory(
        this,
        "Select Destination Parent Folder",
        QFileInfo(m_project.rootPath()).absolutePath());
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Save Project As",
        "New project name:",
        QLineEdit::Normal,
        m_project.name(),
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const Project copy =
        m_project.saveAs(destination, projectName, &errorMessage);
    if (!copy.isValid()) {
        QMessageBox::warning(this, "Save Project As", errorMessage);
        return;
    }

    loadProject(copy);
    statusBar()->showMessage(
        QStringLiteral("Project saved as: %1").arg(destination), 5000);
}

void MainWindow::saveProjectAsCleanCopy()
{
    if (!m_project.isValid())
        return;

    saveProjectSession();

    const QString parentPath = QFileDialog::getExistingDirectory(
        this,
        "Select Destination Parent Folder",
        QFileInfo(m_project.rootPath()).absolutePath());
    if (parentPath.isEmpty())
        return;

    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        this,
        "Save Project As Clean Copy",
        "New project name:",
        QLineEdit::Normal,
        m_project.name(),
        &accepted).trimmed();
    if (!accepted || projectName.isEmpty())
        return;

    ProjectCleanupDialog cleanupDialog(
        m_project.buildCleanupPlan(), m_project.description(), this);
    if (cleanupDialog.exec() != QDialog::Accepted)
        return;

    const QString destination = QDir(parentPath).filePath(projectName);
    QString errorMessage;
    const ProjectCleanupPlan selectedPlan = cleanupDialog.selectedPlan();
    Project copy = m_project.saveAsCleanCopy(
        destination,
        projectName,
        selectedPlan,
        cleanupDialog.selectedDescription(),
        &errorMessage);
    if (!copy.isValid()) {
        QMessageBox::warning(
            this, "Save Project As Clean Copy", errorMessage);
        return;
    }

    loadProject(copy);
    statusBar()->showMessage(
        QStringLiteral("Clean project copy created: %1")
            .arg(destination),
        5000);
}

void MainWindow::cleanProject()
{
    if (!m_project.isValid())
        return;

    ProjectCleanDialog dialog(m_project.buildContentCleanPlan(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ProjectContentCleanPlan selectedPlan = dialog.selectedPlan();
    const int selectedFiles = selectedPlan.totalSelectedFiles();
    const int selectedFolders = selectedPlan.totalSelectedFolders();
    if (selectedFiles <= 0)
        return;

    const auto answer = QMessageBox::question(
        this,
        "Clean Project",
        QStringLiteral(
            "Remove selected contents from the current project?\n\n"
            "This will delete %1 file(s) from %2 folder(s).\n"
            "Collection descriptions and styles will be preserved.\n\n"
            "This cannot be undone.")
            .arg(selectedFiles)
            .arg(selectedFolders),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString errorMessage;
    if (!m_project.applyContentCleanPlan(selectedPlan, &errorMessage)) {
        QMessageBox::warning(
            this,
            "Clean Project",
            errorMessage.isEmpty()
                ? QStringLiteral("The project could not be cleaned.")
                : errorMessage);
        return;
    }

    updateProjectExplorer();
    updateProjectActions();
    statusBar()->showMessage(
        QStringLiteral("Cleaned %1 file(s) from %2 folder(s)")
            .arg(selectedFiles)
            .arg(selectedFolders),
        5000);
}

void MainWindow::editProjectDescription()
{
    if (!m_project.isValid())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle("Edit project description");
    dialog.setMinimumWidth(760);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *descriptionEdit = new QLineEdit(&dialog);
    descriptionEdit->setMinimumWidth(620);
    descriptionEdit->setText(m_project.description());
    descriptionEdit->selectAll();
    form->addRow("Description:", descriptionEdit);

    auto *warningLabel = new QLabel(&dialog);
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("color: #8a1f11;");
    warningLabel->hide();
    form->addRow(QString(), warningLabel);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    auto *okButton = buttons->button(QDialogButtonBox::Ok);
    layout->addWidget(buttons);

    const auto updateOkButton = [&]() {
        QString descriptionError;
        const bool descriptionOk =
            Project::descriptionIsSupported(
                descriptionEdit->text().trimmed(),
                &descriptionError);
        warningLabel->setVisible(!descriptionOk);
        warningLabel->setText(descriptionError);
        okButton->setEnabled(descriptionOk);
    };

    connect(descriptionEdit, &QLineEdit::textChanged,
            &dialog, updateOkButton);
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);
    updateOkButton();

    if (dialog.exec() != QDialog::Accepted)
        return;

    QString errorMessage;
    if (!m_project.setDescription(
            descriptionEdit->text(),
            &errorMessage)) {
        QMessageBox::warning(
            this,
            "Edit project description",
            errorMessage.isEmpty()
                ? QStringLiteral("The project description could not be saved.")
                : errorMessage);
        return;
    }

    updateProjectHeader();
    if (m_projectFileSystemModel)
        m_projectFileSystemModel->refreshDirectory(m_project.rootPath());
    statusBar()->showMessage("Project description updated", 3000);
}

void MainWindow::createProjectReport()
{
    if (!m_project.isValid())
        return;

    const QString reportPath = m_project.reportFilePath();
    if (QFileInfo::exists(reportPath)) {
        const auto answer = QMessageBox::question(
            this, "Project Report",
            "Replace the existing project report?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    QString errorMessage;
    if (!m_project.writeReport(&errorMessage)) {
        QMessageBox::warning(
            this, "Project Report", errorMessage);
        return;
    }

    m_projectFileSystemModel->refreshDirectory(m_project.rootPath());
    const auto openAnswer = QMessageBox::question(
        this, "Project Report",
        "The report was created as project-report.md.\n"
        "Open it with the default application?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (openAnswer == QMessageBox::Yes) {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(reportPath))) {
            QMessageBox::warning(
                this, "Project Report",
                "The report was created but could not be opened.");
        }
    }
}

void MainWindow::openImage()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this, "Open Image",
        m_project.isValid() ? m_project.rootPath() : QString(),
        "Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp)");

    for (const QString &fileName : fileNames)
        openImageAtPath(fileName);
}

void MainWindow::importRaw()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Import raw",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QStringList fileNames = QFileDialog::getOpenFileNames(
        this, "Select RAW files to import", QString(),
        "RAW files (*.CR2 *.CR3 *.NEF *.ARW *.DNG *.RAF *.ORF *.RW2 *.PEF *.SRW *.RAW *.DCR *.KDC)");
    if (fileNames.isEmpty())
        return;

    const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
    QDir().mkpath(rawDir);

    int copiedCount = 0;
    for (const QString &srcPath : fileNames) {
        const QFileInfo fi(srcPath);
        const QString destPath = QDir(rawDir).filePath(fi.fileName());
        if (QFile::exists(destPath))
            continue;
        if (QFile::copy(srcPath, destPath))
            ++copiedCount;
    }

    if (copiedCount > 0) {
        m_projectFileSystemModel->refreshDirectory(rawDir);
        statusBar()->showMessage(
            QStringLiteral("Imported %1 RAW file(s)").arg(copiedCount), 5000);
        updateProjectActions();
    } else {
        statusBar()->showMessage("No new RAW files were imported.", 3000);
    }
}

void MainWindow::extractRawData()
{
    if (!m_project.isValid())
        return;

    ui->actionExtractRawData->setEnabled(false);
    statusBar()->showMessage("Scanning RAW files...", 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
    const QString sensorDir = QDir(m_project.rootPath()).filePath("sensor_data");
    QDir().mkpath(sensorDir);

    QStringList rawFiles;
    QDirIterator it(rawDir, QDir::Files, QDirIterator::Subdirectories);
    const QStringList rawExtensions = {"cr2", "cr3", "nef", "arw", "dng", "raf",
                                       "orf", "rw2", "pef", "srw", "raw", "dcr", "kdc"};
    while (it.hasNext()) {
        const QString path = it.next();
        const QString ext = QFileInfo(path).suffix().toLower();
        if (rawExtensions.contains(ext))
            rawFiles.append(path);
    }

    struct RawExtractionJob {
        QString rawPath;
        QString tiffPath;
    };
    QVector<RawExtractionJob> extractionJobs;
    extractionJobs.reserve(rawFiles.size());
    for (const QString &rawPath : std::as_const(rawFiles)) {
        const QFileInfo fi(rawPath);
        const QString tiffPath =
            QDir(sensorDir).filePath(fi.completeBaseName() + ".tif");
        if (!QFile::exists(tiffPath))
            extractionJobs.append({rawPath, tiffPath});
    }

    auto *progress = new QProgressBar(statusBar());
    progress->setMinimum(0);
    progress->setMaximum(qMax(1, extractionJobs.size() + 1));
    progress->setValue(0);
    progress->setTextVisible(true);
    progress->setMinimumWidth(180);
    progress->setFormat("Preparing...");
    statusBar()->addPermanentWidget(progress);
    auto finishProgress = [this, progress]() {
        statusBar()->removeWidget(progress);
        progress->deleteLater();
        updateProjectActions();
    };

    int successCount = 0;
    int failCount = 0;
    int processedCount = 0;

    for (const RawExtractionJob &job : std::as_const(extractionJobs)) {
        const QFileInfo fi(job.rawPath);
        ++processedCount;

        progress->setValue(processedCount - 1);
        progress->setFormat(
            QStringLiteral("RAW %1/%2")
                .arg(processedCount)
                .arg(extractionJobs.size()));
        statusBar()->showMessage(
            QStringLiteral("Extracting raw data and metadata: %1/%2 %3")
                .arg(processedCount)
                .arg(extractionJobs.size())
                .arg(fi.fileName()),
            0);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        QString errorMsg;
        if (RawReader::extractSensorData(job.rawPath, job.tiffPath, &errorMsg))
            ++successCount;
        else
            ++failCount;

        progress->setValue(processedCount);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    progress->setValue(extractionJobs.size());
    progress->setFormat("Metadata");
    statusBar()->showMessage(
        "Writing acquisition metadata sidecars...", 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    AcquisitionMetadataSummary metadataSummary;
    QString metadataError;
    const bool metadataWritten = AcquisitionMetadata::regenerate(
        m_project.rootPath(), &metadataSummary, &metadataError);
    progress->setValue(progress->maximum());
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    finishProgress();
    statusBar()->showMessage(
        QStringLiteral(
            "Extraction complete: %1 TIFF(s) created, %2 failed; "
            "%3 metadata sidecar(s) written")
            .arg(successCount)
            .arg(failCount)
            .arg(metadataSummary.sidecarsWritten),
        8000);
    m_projectFileSystemModel->refreshDirectory(sensorDir);

    if (!metadataWritten || metadataSummary.metadataErrors > 0) {
        QStringList details;
        if (metadataSummary.metadataErrors > 0) {
            details.append(
                QStringLiteral(
                    "%1 sidecar(s) record a metadata error.")
                    .arg(metadataSummary.metadataErrors));
        }
        if (metadataSummary.writeErrors > 0) {
            details.append(
                QStringLiteral("%1 sidecar(s) could not be written.")
                    .arg(metadataSummary.writeErrors));
        }
        if (!metadataError.isEmpty())
            details.append(metadataError);
        details.append(
            "Inspect the TSV files in sensor_data for per-image details.");
        QMessageBox::warning(
            this, "Extract raw data and metadata", details.join("\n"));
    }

    refreshReloadableArtifactWatchers();
}

void MainWindow::importLegacyJpegs()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Import legacy JPEGs",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    QVector<Collection> sourceCollections;
    QVector<Collection> destinationCollections;
    for (const Collection &collection : collections) {
        if (collection.collectionClass == "other"
            && !LegacyJpegImporter::jpegFiles(collection.dirPath).isEmpty()) {
            sourceCollections.append(collection);
        } else if (collection.collectionClass
                   == "preprocessed_image_collection") {
            destinationCollections.append(collection);
        }
    }

    if (sourceCollections.isEmpty()) {
        QMessageBox::information(
            this,
            "Import legacy JPEGs",
            "No `other` collection with JPEG files was found.\n\n"
            "Place legacy JPEG files in a folder registered as an `other` collection first.");
        return;
    }
    if (destinationCollections.isEmpty()) {
        QMessageBox::information(
            this,
            "Import legacy JPEGs",
            "No preprocessed image collection is registered in this project.\n\n"
            "Register a destination folder as a `preprocessed_image_collection` first.");
        return;
    }

    LegacyJpegImportDialog dialog(
        sourceCollections, destinationCollections, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Collection source = dialog.sourceCollection();
    const Collection destination = dialog.destinationCollection();
    if (source.dirPath.isEmpty() || destination.dirPath.isEmpty())
        return;

    LegacyJpegImportOptions options;
    options.projectRoot = m_project.rootPath();
    options.sourceCollectionDir = source.dirPath;
    options.sourceCollectionName = source.dirName;
    options.destinationCollectionDir = destination.dirPath;
    options.destinationCollectionName = destination.dirName;

    const QStringList conflicts =
        LegacyJpegImporter::conflictingOutputs(options);
    if (!conflicts.isEmpty()) {
        const auto reply = QMessageBox::question(
            this,
            "Import legacy JPEGs",
            QStringLiteral(
                "%1 output file(s) already exist in the destination collection.\n\n"
                "Replace existing TIFF/TSV files?")
                .arg(conflicts.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        options.replaceExisting = true;
    }

    statusBar()->showMessage("Importing legacy JPEGs as TIFF...", 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    LegacyJpegImportReport report;
    QString errorMessage;
    const bool imported =
        LegacyJpegImporter::importProject(options, &report, &errorMessage);

    m_projectFileSystemModel->refreshDirectory(destination.dirPath);
    updateProjectActions();
    statusBar()->showMessage(
        QStringLiteral("Legacy JPEG import: %1 TIFF(s), %2 failed")
            .arg(report.convertedFiles)
            .arg(report.failedFiles),
        8000);

    QStringList details;
    if (!imported && !errorMessage.isEmpty())
        details.append(errorMessage);
    details.append(
        QStringLiteral("JPEG files found: %1").arg(report.sourceFiles));
    details.append(
        QStringLiteral("TIFF files written: %1").arg(report.convertedFiles));
    details.append(
        QStringLiteral("Failed conversions: %1").arg(report.failedFiles));
    details.append(
        QStringLiteral("Metadata sidecars written: %1")
            .arg(report.sidecarsWritten));
    details.append(
        QStringLiteral("Metadata errors: %1").arg(report.metadataErrors));
    if (!report.warnings.isEmpty()) {
        details.append(QString());
        details.append("Warnings:");
        details.append(report.warnings);
    }
    details.append(QString());
    details.append("Inspect legacy_jpeg_import.md in the destination collection.");

    if (!imported || report.failedFiles > 0 || report.metadataErrors > 0
        || !report.warnings.isEmpty()) {
        QMessageBox::warning(
            this, "Import legacy JPEGs", details.join("\n"));
    } else {
        QMessageBox::information(
            this, "Import legacy JPEGs", details.join("\n"));
    }
}

void MainWindow::registerCollection()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Register collection", "No project is open.");
        return;
    }

    const QVector<Collection> existingCollections = Project::discoverCollections(m_project.rootPath());
    CollectionDialog dialog(m_project.rootPath(), existingCollections, selectedProjectPath(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString dirPath = dialog.selectedDirectory();
    if (dirPath.isEmpty())
        return;

    const QString selectedDirName = QDir(dirPath).dirName();
    if (Project::reservedDirectories().contains(selectedDirName, Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Register collection",
                             QStringLiteral("Cannot register collection: \"%1\" is a reserved directory.").arg(selectedDirName));
        return;
    }

    const QString yamlPath = QDir(dirPath).filePath("collection.yaml");
    if (QFileInfo::exists(yamlPath)) {
        const auto reply = QMessageBox::question(
            this, "Register collection",
            "Collection configuration for this folder already exists. Overwrite it?");
        if (reply != QMessageBox::Yes)
            return;
    }

    QString errorMsg;
    if (Project::registerCollection(m_project.rootPath(),
                                     dirPath,
                                     dialog.collectionClass(),
                                     dialog.parent(),
                                     dialog.related(),
                                     dialog.registry(),
                                     dialog.script(),
                                     dialog.alias(),
                                     &errorMsg))
    {
        m_projectFileSystemModel->refreshDirectory(dirPath);
        statusBar()->showMessage(
            QStringLiteral("Collection registered: %1").arg(QDir(dirPath).dirName()), 5000);
    } else {
        QMessageBox::warning(this, "Register collection", errorMsg);
    }
}

void MainWindow::checkCollections()
{
    if (!m_project.isValid()) {
        QMessageBox::information(
            this, "Check collections", "No project is open.");
        return;
    }

    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    const CollectionCheckResult result =
        checkRegisteredCollections(m_project.rootPath(), collections);
    showCollectionCheckDialog(this, result);

    if (result.errors.isEmpty() && result.warnings.isEmpty()) {
        statusBar()->showMessage("Collection check found no problems.", 5000);
    } else {
        statusBar()->showMessage(
            QStringLiteral(
                "Collection check found %1 error(s) and %2 warning(s)")
                .arg(result.errors.size())
                .arg(result.warnings.size()),
            7000);
    }
}

void MainWindow::showCollectionGraph()
{
    if (!m_project.isValid()) {
        QMessageBox::information(
            this, "Collection graph", "No project is open.");
        return;
    }

    if (m_collectionGraphDialog) {
        m_collectionGraphDialog->show();
        m_collectionGraphDialog->raise();
        m_collectionGraphDialog->activateWindow();
        return;
    }

    m_collectionGraphDialog =
        new CollectionGraphDialog(m_project.rootPath(), this);
    m_collectionGraphDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_collectionGraphDialog->show();
}

void MainWindow::onProjectTreeDoubleClicked(const QModelIndex &index)
{
    const QString filePath = projectPathFromIndex(index);
    if (filePath.isEmpty())
        return;

    const QFileInfo fi(filePath);

    const QStringList imageExtensions = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    if (imageExtensions.contains(fi.suffix().toLower())) {
        openImageAtPath(filePath);
        return;
    }
}

void MainWindow::onProjectTreeContextMenu(const QPoint &position)
{
    if (!m_project.isValid())
        return;

    const QModelIndex index = ui->projectTree->indexAt(position);
    if (!index.isValid())
        return;

    const QModelIndex rowIndex = index.sibling(index.row(), 0);
    if (!ui->projectTree->selectionModel()->isSelected(rowIndex)) {
        ui->projectTree->setCurrentIndex(rowIndex);
        ui->projectTree->selectionModel()->select(
            rowIndex,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        ui->projectTree->setCurrentIndex(rowIndex);
    }

    const QString itemPath = projectPathFromIndex(rowIndex);
    if (itemPath.isEmpty())
        return;

    const QFileInfo info(itemPath);

    QMenu menu(this);
    QAction *openDefaultAction =
        menu.addAction("Open with default app");
    openDefaultAction->setEnabled(info.isFile());
    QAction *showInFolderAction =
        menu.addAction("Show in folder");
    menu.addSeparator();
    QAction *deleteAction = menu.addAction("Delete");

    QAction *registerAction = nullptr;
    QAction *editCollectionAction = nullptr;
    QAction *renameCollectionAction = nullptr;
    QAction *unregisterCollectionAction = nullptr;
    QAction *createPointsAction = nullptr;
    QAction *createMaskAction = nullptr;
    QAction *styleAction = nullptr;
    QAction *copyStyleAction = nullptr;
    QAction *pasteStyleAction = nullptr;
    QAction *validateCollectionAction = nullptr;
    QAction *unregisterDescriptorAction = nullptr;
    if (info.isDir()) {
        menu.addSeparator();
        registerAction = menu.addAction("Register collection...");
        editCollectionAction = menu.addAction("Edit collection...");
        renameCollectionAction =
            menu.addAction("Rename collection folder...");
        unregisterCollectionAction =
            menu.addAction("Unregister collection...");
        createPointsAction =
            menu.addAction("Create empty point layer...");
        createMaskAction =
            menu.addAction("Create empty mask...");
        styleAction = menu.addAction("Edit collection style...");
        copyStyleAction = menu.addAction("Copy collection style");
        pasteStyleAction = menu.addAction("Paste collection style");
        validateCollectionAction =
            menu.addAction("Validate collection...");
    }

    const bool isReserved =
        Project::reservedDirectories().contains(
            info.fileName(), Qt::CaseInsensitive);
    Collection selectedCollection;
    bool hasRegisteredCollection = false;
    Collection unregisterTargetCollection;
    bool hasUnregisterTargetCollection = false;
    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    const int selectedCollectionIndex = collectionIndexByDirectory(
        m_project.rootPath(), collections, info.absoluteFilePath());
    if (selectedCollectionIndex >= 0) {
        selectedCollection = collections.at(selectedCollectionIndex);
        hasRegisteredCollection = true;
        unregisterTargetCollection = selectedCollection;
        hasUnregisterTargetCollection = true;
    }

    const bool isCollectionDescriptor =
        info.isFile()
        && info.fileName().compare(
            "collection.yaml", Qt::CaseInsensitive) == 0;
    if (!hasUnregisterTargetCollection && isCollectionDescriptor) {
        const int descriptorCollectionIndex = collectionIndexByDirectory(
            m_project.rootPath(),
            collections,
            info.absoluteDir().absolutePath());
        if (descriptorCollectionIndex >= 0) {
            unregisterTargetCollection =
                collections.at(descriptorCollectionIndex);
            hasUnregisterTargetCollection = true;
        }
    }

    if (registerAction)
        registerAction->setEnabled(!isReserved && !hasRegisteredCollection);
    if (deleteAction && hasUnregisterTargetCollection) {
        deleteAction->setEnabled(false);
        deleteAction->setToolTip(
            "Use Unregister collection... before deleting collection "
            "folders or collection.yaml.");
    }
    const bool supportsStyle = info.isDir() && hasRegisteredCollection
        && selectedCollection.collectionClass != "other";
    const QString selectedStyleKind =
        hasRegisteredCollection ? collectionStyleKind(selectedCollection) : QString();
    const CollectionStyleData selectedStyle = supportsStyle
        ? CollectionStyleFile::load(selectedCollection.dirPath)
        : CollectionStyleData();
    const bool hasValidPersistedStyle = supportsStyle
        && collectionStyleMatchesKind(selectedStyle, selectedStyleKind);
    const bool canPasteStyle = supportsStyle
        && !m_copiedCollectionStyleKind.isEmpty()
        && m_copiedCollectionStyleKind == selectedStyleKind
        && collectionStyleMatchesKind(
            m_copiedCollectionStyle, m_copiedCollectionStyleKind);
    const bool supportsCreatePoints = info.isDir() && hasRegisteredCollection
        && selectedCollection.collectionClass == "point_collection";
    const bool supportsCreateMask = info.isDir() && hasRegisteredCollection
        && selectedCollection.collectionClass == "segmentation_collection";
    const bool supportsCollectionValidation =
        info.isDir() && hasRegisteredCollection
        && CollectionValidation::isEligible(
            m_project.rootPath(), selectedCollection);
    if (createPointsAction)
        createPointsAction->setEnabled(supportsCreatePoints);
    if (editCollectionAction)
        editCollectionAction->setEnabled(hasRegisteredCollection);
    if (renameCollectionAction)
        renameCollectionAction->setEnabled(hasRegisteredCollection);
    if (unregisterCollectionAction)
        unregisterCollectionAction->setEnabled(hasRegisteredCollection);
    if (createMaskAction)
        createMaskAction->setEnabled(supportsCreateMask);
    if (styleAction)
        styleAction->setEnabled(supportsStyle);
    if (copyStyleAction)
        copyStyleAction->setEnabled(hasValidPersistedStyle);
    if (pasteStyleAction)
        pasteStyleAction->setEnabled(canPasteStyle);
    if (validateCollectionAction)
        validateCollectionAction->setEnabled(
            supportsCollectionValidation);

    QAction *validateImageAction = nullptr;
    if (info.isFile()) {
        if (isCollectionDescriptor && hasUnregisterTargetCollection) {
            menu.addSeparator();
            unregisterDescriptorAction =
                menu.addAction("Unregister collection...");
        }

        const QString extension = info.suffix().toLower();
        const bool rawFile =
            QStringList{
                "cr2", "cr3", "nef", "arw", "dng", "raf", "orf",
                "rw2", "pef", "srw", "raw", "dcr", "kdc"
            }.contains(extension);
        const Collection fileCollection =
            collectionForPath(info.absoluteFilePath());
        const bool preprocessedImage =
            CollectionValidation::isEligible(
                m_project.rootPath(), fileCollection)
            && CollectionValidation::preprocessedImageFilters().contains(
                "*." + info.suffix(), Qt::CaseInsensitive);
        if (rawFile || preprocessedImage) {
            menu.addSeparator();
            validateImageAction =
                menu.addAction("Validate image...");
        }
    }

    connect(openDefaultAction, &QAction::triggered, this, [this, info]() {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(info.absoluteFilePath()))) {
            QMessageBox::warning(
                this, "Open with default app",
                "The selected file could not be opened.");
        }
    });
    connect(showInFolderAction, &QAction::triggered, this, [this, info]() {
#ifdef Q_OS_WIN
        const QString path = QDir::toNativeSeparators(
            info.absoluteFilePath());
        const QStringList arguments = info.isDir()
            ? QStringList{path}
            : QStringList{QStringLiteral("/select,%1").arg(path)};
        if (!QProcess::startDetached(
                QStringLiteral("explorer.exe"), arguments)) {
            QMessageBox::warning(
                this, "Show in folder",
                "The selected location could not be opened.");
        }
#else
        const QString folderPath = info.isDir()
            ? info.absoluteFilePath()
            : info.absoluteDir().absolutePath();
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(folderPath))) {
            QMessageBox::warning(
                this, "Show in folder",
                "The selected location could not be opened.");
        }
#endif
    });
    connect(deleteAction, &QAction::triggered, this, [this, info]() {
        const QString kind = info.isDir() ? "folder" : "file";
        const auto answer = QMessageBox::warning(
            this, "Delete",
            QStringLiteral("Delete the %1 \"%2\"%3?")
                .arg(kind, info.fileName(),
                     info.isDir() ? " and all its contents" : ""),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;

        const QString parentPath = info.absoluteDir().absolutePath();
        const ProjectExplorerViewState viewState =
            projectExplorerViewState();
        const bool removed = info.isDir()
            ? QDir(info.absoluteFilePath()).removeRecursively()
            : QFile::remove(info.absoluteFilePath());
        if (!removed) {
            QMessageBox::warning(
                this, "Delete", "The selected item could not be deleted.");
            return;
        }
        const QString deletedRelativePath = projectRelativePath(
            info.absoluteFilePath());
        if (!deletedRelativePath.isEmpty()
            && deletedRelativePath != ".") {
            const QString childPrefix = deletedRelativePath + "/";
            const QStringList markedPaths = m_projectFileMarks.values();
            for (const QString &relativePath : markedPaths) {
                if (relativePath == deletedRelativePath
                    || relativePath.startsWith(childPrefix)) {
                    m_projectFileMarks.remove(relativePath);
                }
            }
            saveProjectFileMarks();
            m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        }
        if (m_previewDock)
            m_previewDock->clear();
        m_projectFileSystemModel->refreshDirectory(parentPath);
        QTimer::singleShot(0, this, [this, viewState]() {
            restoreProjectExplorerViewState(viewState);
        });
        updateProjectActions();
    });

    if (registerAction) {
        connect(registerAction, &QAction::triggered,
                this, &MainWindow::registerCollection);
    }
    if (editCollectionAction) {
        connect(editCollectionAction, &QAction::triggered,
                this, [this, selectedCollection, hasRegisteredCollection]() {
                    if (!hasRegisteredCollection)
                        return;

                    const QVector<Collection> collections =
                        Project::discoverCollections(m_project.rootPath());
                    CollectionEditDialog dialog(
                        m_project.rootPath(),
                        selectedCollection,
                        collections,
                        this);
                    if (dialog.exec() != QDialog::Accepted)
                        return;

                    const ProjectExplorerViewState viewState =
                        projectExplorerViewState();
                    QString errorMessage;
                    if (!Project::updateCollectionMetadata(
                            m_project.rootPath(),
                            selectedCollection.dirPath,
                            dialog.alias(),
                            dialog.parentReference(),
                            dialog.relatedReference(),
                            dialog.registry(),
                            dialog.script(),
                            &errorMessage)) {
                        QMessageBox::warning(
                            this, "Edit collection", errorMessage);
                        return;
                    }

                    m_projectFileSystemModel->refreshDirectory(
                        selectedCollection.dirPath);
                    QTimer::singleShot(0, this, [this, viewState]() {
                        restoreProjectExplorerViewState(viewState);
                    });
                    statusBar()->showMessage(
                        QStringLiteral("Updated collection: %1")
                            .arg(selectedCollection.dirName),
                        3000);
                });
    }
    if (renameCollectionAction) {
        connect(renameCollectionAction, &QAction::triggered,
                this, [this, selectedCollection, hasRegisteredCollection]() {
                    if (!hasRegisteredCollection)
                        return;

                    const QString projectRoot = m_project.rootPath();
                    const QVector<Collection> collections =
                        Project::discoverCollections(projectRoot);
                    const QString oldDirectoryPath =
                        selectedCollection.dirPath;
                    const QStringList metadataReferences =
                        collectionRenameReferenceLabels(
                            projectRoot, collections, oldDirectoryPath);
                    const QStringList scriptMentions =
                        scriptFilesMentioningCollection(
                            projectRoot, oldDirectoryPath);
                    const QStringList openTabs =
                        workspaceTabLabelsTouchingDirectory(
                            ui->workspaceTabs, oldDirectoryPath);

                    const RenameCollectionDecision decision =
                        confirmRenameCollection(
                            this,
                            projectRoot,
                            selectedCollection,
                            metadataReferences,
                            scriptMentions,
                            openTabs);
                    if (!decision.accepted)
                        return;

                    const QString newDirectoryPath =
                        QDir(projectRoot).filePath(decision.newName);
                    const QVector<int> tabsToClose =
                        workspaceTabIndexesTouchingDirectory(
                            ui->workspaceTabs, oldDirectoryPath);
                    if (!tabsToClose.isEmpty())
                        closeAttributeTables();
                    for (int i = tabsToClose.size() - 1; i >= 0; --i)
                        onWorkspaceTabCloseRequested(tabsToClose.at(i));

                    QString errorMessage;
                    if (!renameProjectChildDirectory(
                            projectRoot,
                            selectedCollection.dirName,
                            decision.newName,
                            &errorMessage)) {
                        QMessageBox::warning(
                            this, "Rename collection folder", errorMessage);
                        return;
                    }

                    int updatedCollections = 0;
                    if (!updateCollectionReferencesAfterRename(
                            projectRoot,
                            collections,
                            oldDirectoryPath,
                            newDirectoryPath,
                            decision.newName,
                            &updatedCollections,
                            &errorMessage)) {
                        QMessageBox::warning(
                            this,
                            "Rename collection folder",
                            errorMessage.isEmpty()
                                ? QStringLiteral(
                                      "The folder was renamed, but some "
                                      "collection references could not be updated.")
                                : errorMessage);
                    }

                    int remappedMarks = 0;
                    const QString oldRelativePath =
                        QDir::fromNativeSeparators(
                            QDir(projectRoot).relativeFilePath(
                                oldDirectoryPath));
                    const QString newRelativePath =
                        QDir::fromNativeSeparators(
                            QDir(projectRoot).relativeFilePath(
                                newDirectoryPath));
                    if (!oldRelativePath.isEmpty()
                        && oldRelativePath != "."
                        && !newRelativePath.isEmpty()
                        && newRelativePath != ".") {
                        const QString childPrefix = oldRelativePath + "/";
                        QSet<QString> remapped;
                        for (const QString &relativePath
                             : std::as_const(m_projectFileMarks)) {
                            if (relativePath == oldRelativePath) {
                                remapped.insert(newRelativePath);
                                ++remappedMarks;
                            } else if (relativePath.startsWith(childPrefix)) {
                                remapped.insert(
                                    newRelativePath
                                    + relativePath.mid(
                                        oldRelativePath.size()));
                                ++remappedMarks;
                            } else {
                                remapped.insert(relativePath);
                            }
                        }
                        if (remappedMarks > 0) {
                            m_projectFileMarks = remapped;
                            saveProjectFileMarks();
                            if (m_projectFileSystemModel)
                                m_projectFileSystemModel->setFileMarks(
                                    m_projectFileMarks);
                        }
                    }

                    if (m_previewDock)
                        m_previewDock->clear();
                    updateProjectExplorer();
                    m_projectFileSystemModel->refreshDirectory(projectRoot);
                    m_projectFileSystemModel->refreshDirectory(
                        newDirectoryPath);
                    static_cast<ProjectExplorerSortProxyModel*>(
                        m_projectSortProxy)->invalidate();
                    refreshProjectExplorerRootIndex();
                    saveProjectSession();
                    refreshReloadableArtifactWatchers();
                    updateProjectActions();

                    statusBar()->showMessage(
                        QStringLiteral(
                            "Renamed collection %1 to %2; updated %3 collection file(s), remapped %4 star(s), closed %5 tab(s)")
                            .arg(selectedCollection.dirName,
                                 decision.newName)
                            .arg(updatedCollections)
                            .arg(remappedMarks)
                            .arg(tabsToClose.size()),
                        8000);
                });
    }
    const auto runUnregisterCollection =
        [this, unregisterTargetCollection,
         hasUnregisterTargetCollection]() {
            if (!hasUnregisterTargetCollection)
                return;

            const UnregisterImpact impact = analyzeUnregisterImpact(
                m_project.rootPath(),
                unregisterTargetCollection,
                Project::discoverCollections(m_project.rootPath()));
            const UnregisterDecision decision =
                confirmUnregisterCollection(
                    this, unregisterTargetCollection, impact);
            if (!decision.accepted)
                return;

            QString errorMessage;
            int updatedDependencies = 0;
            if (!applyUnregisterDependencyCleanup(
                    m_project.rootPath(),
                    impact,
                    decision,
                    &updatedDependencies,
                    &errorMessage)) {
                QMessageBox::warning(
                    this, "Unregister collection", errorMessage);
                return;
            }

            if (!Project::unregisterCollection(
                    m_project.rootPath(),
                    unregisterTargetCollection.dirPath,
                    &errorMessage)) {
                QMessageBox::warning(
                    this, "Unregister collection", errorMessage);
                return;
            }

            if (m_previewDock)
                m_previewDock->clear();
            m_projectFileSystemModel->refreshDirectory(
                unregisterTargetCollection.dirPath);
            updateProjectActions();
            const QString message = updatedDependencies > 0
                ? QStringLiteral(
                      "Unregistered collection: %1; updated %2 dependent collection file(s)")
                      .arg(unregisterTargetCollection.dirName)
                      .arg(updatedDependencies)
                : QStringLiteral("Unregistered collection: %1")
                      .arg(unregisterTargetCollection.dirName);
            statusBar()->showMessage(message, 5000);
        };
    if (unregisterCollectionAction) {
        connect(unregisterCollectionAction,
                &QAction::triggered,
                this,
                runUnregisterCollection);
    }
    if (unregisterDescriptorAction) {
        connect(unregisterDescriptorAction,
                &QAction::triggered,
                this,
                runUnregisterCollection);
    }
    if (createPointsAction) {
        connect(createPointsAction, &QAction::triggered,
                this, &MainWindow::createPointsCsv);
    }
    if (createMaskAction) {
        connect(createMaskAction, &QAction::triggered,
                this, [this, selectedCollection, supportsCreateMask]() {
                    if (!supportsCreateMask)
                        return;
                    createMaskTiffInCollection(selectedCollection);
                });
    }
    if (styleAction) {
        connect(styleAction, &QAction::triggered,
                this, [this, selectedCollection, supportsStyle]() {
                    if (!supportsStyle)
                        return;
                    CollectionStyleDialog dialog(selectedCollection, this);
                    if (dialog.exec() == QDialog::Accepted)
                        m_projectFileSystemModel->refreshDirectory(
                            selectedCollection.dirPath);
                });
    }
    if (copyStyleAction) {
        connect(copyStyleAction, &QAction::triggered,
                this, [this, selectedCollection, selectedStyle,
                       selectedStyleKind, hasValidPersistedStyle]() {
                    if (!hasValidPersistedStyle)
                        return;
                    m_copiedCollectionStyle = selectedStyle;
                    m_copiedCollectionStyleKind = selectedStyleKind;
                    m_copiedCollectionStyleSource =
                        selectedCollection.dirName;
                    statusBar()->showMessage(
                        QStringLiteral("Copied collection style from %1")
                            .arg(selectedCollection.dirName),
                        3000);
                });
    }
    if (pasteStyleAction) {
        connect(pasteStyleAction, &QAction::triggered,
                this, [this, selectedCollection, selectedStyleKind,
                       canPasteStyle]() {
                    if (!canPasteStyle)
                        return;
                    QString errorMessage;
                    if (!CollectionStyleFile::save(
                            selectedCollection.dirPath,
                            m_copiedCollectionStyle,
                            &errorMessage)) {
                        QMessageBox::warning(
                            this, "Paste Collection Style",
                            errorMessage);
                        return;
                    }
                    m_projectFileSystemModel->refreshDirectory(
                        selectedCollection.dirPath);
                    statusBar()->showMessage(
                        QStringLiteral("Pasted %1 collection style to %2")
                            .arg(selectedStyleKind,
                                 selectedCollection.dirName),
                        3000);
                });
    }
    if (validateCollectionAction) {
        connect(validateCollectionAction, &QAction::triggered,
                this, &MainWindow::validateCollectionMetadata);
    }
    if (validateImageAction) {
        connect(validateImageAction, &QAction::triggered,
                this, [this, info]() {
                    const Collection collection =
                        collectionForPath(info.absoluteFilePath());
                    if (CollectionValidation::isEligible(
                            m_project.rootPath(), collection)) {
                        const QList<CollectionFileValidation> pairs =
                            CollectionValidation::resolveRawPairs(
                                m_project.rootPath(), collection);
                        for (const CollectionFileValidation &pair : pairs) {
                            if (!samePath(
                                    pair.preprocessedPath,
                                    info.absoluteFilePath())) {
                                continue;
                            }
                            if (pair.status
                                == CollectionFileValidationStatus::MissingRaw) {
                                QMessageBox::warning(
                                    this, "Validate Image",
                                    QString(
                                        "No RAW with basename \"%1\" was "
                                        "found in raw_files.")
                                        .arg(pair.baseName));
                                return;
                            }
                            if (pair.status
                                == CollectionFileValidationStatus::AmbiguousRaw) {
                                QMessageBox::warning(
                                    this, "Validate Image",
                                    QString(
                                        "More than one RAW with basename "
                                        "\"%1\" was found:\n%2")
                                        .arg(pair.baseName,
                                             pair.rawCandidates.join("\n")));
                                return;
                            }
                            validateRawImage(
                                pair.rawPath,
                                [&]() {
                                    QString registryPath;
                                    CollectionValidation::resolveRegistry(
                                        m_project.rootPath(),
                                        collection, &registryPath);
                                    return registryPath;
                                }());
                            return;
                        }
                    }
                    validateRawImage(info.absoluteFilePath());
                });
    }

    menu.exec(ui->projectTree->viewport()->mapToGlobal(position));
}

void MainWindow::createProjectFolder()
{
    if (!m_project.isValid())
        return;

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, "New folder", "Folder name:",
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    if (name == "." || name == ".."
        || name.contains('/') || name.contains('\\')) {
        QMessageBox::warning(
            this, "New folder", "Enter a simple folder name.");
        return;
    }

    QDir root(m_project.rootPath());
    if (QFileInfo::exists(root.filePath(name))) {
        QMessageBox::warning(
            this, "New folder",
            "A file or folder with that name already exists.");
        return;
    }
    if (!root.mkdir(name)) {
        QMessageBox::warning(
            this, "New folder", "The folder could not be created.");
        return;
    }

    const ProjectExplorerViewState viewState =
        projectExplorerViewState();
    m_projectFileSystemModel->refreshDirectory(m_project.rootPath());
    QTimer::singleShot(0, this, [this, viewState]() {
        restoreProjectExplorerViewState(viewState);
    });
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool viewerDropArea =
        watched == m_viewerEmptyState
        || watched == ui->workspaceTabs
        || (ui->workspaceTabs && watched == ui->workspaceTabs->tabBar());
    if (viewerDropArea) {
        if (event->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::DragMove) {
            auto *dragEvent = static_cast<QDragMoveEvent*>(event);
            if (dragEvent->mimeData()->hasUrls())
                dragEvent->acceptProposedAction();
            else
                dragEvent->ignore();
            return true;
        }
        if (event->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent*>(event));
            updateViewerEmptyState();
            return true;
        }
    }

    if (watched != ui->projectTree->viewport()
        || !m_project.isValid()) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex markIndex;
        if (mouseEvent->button() == Qt::LeftButton
            && projectFileMarkHitTest(
                mouseEvent->position().toPoint(), &markIndex)) {
            if (event->type() == QEvent::MouseButtonPress)
                toggleProjectFileMark(projectPathFromIndex(markIndex));
            return true;
        }
    }

    if (event->type() == QEvent::DragEnter
        || event->type() == QEvent::DragMove) {
        auto *dragEvent = static_cast<QDragMoveEvent*>(event);
        if (dragEvent->mimeData()->hasUrls()) {
            bool hasLocalFile = false;
            for (const QUrl &url : dragEvent->mimeData()->urls()) {
                const QFileInfo info(url.toLocalFile());
                if (info.isFile()) {
                    hasLocalFile = true;
                    break;
                }
            }
            if (hasLocalFile) {
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        }
    } else if (event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent*>(event);
        const QModelIndex index =
            ui->projectTree->indexAt(dropEvent->position().toPoint());
        if (!index.isValid()) {
            dropEvent->ignore();
            return true;
        }

        const QString destinationPath = projectPathFromIndex(index);
        if (destinationPath.isEmpty()) {
            dropEvent->ignore();
            return true;
        }

        QFileInfo destination(destinationPath);
        const QString destinationDirectory = destination.isDir()
            ? destination.absoluteFilePath()
            : destination.absoluteDir().absolutePath();
        if (copyDroppedFiles(
                dropEvent->mimeData()->urls(), destinationDirectory)) {
            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
        } else {
            dropEvent->ignore();
        }
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::copyDroppedFiles(
    const QList<QUrl> &urls,
    const QString &destinationDirectory)
{
    QDir destination(destinationDirectory);
    if (!destination.exists())
        return false;

    int copied = 0;
    for (const QUrl &url : urls) {
        const QFileInfo source(url.toLocalFile());
        if (!source.isFile())
            continue;

        const QString targetPath =
            destination.filePath(source.fileName());
        if (QFileInfo(targetPath).absoluteFilePath()
            == source.absoluteFilePath()) {
            continue;
        }

        if (QFileInfo::exists(targetPath)) {
            const auto answer = QMessageBox::question(
                this, "Replace file",
                QStringLiteral("\"%1\" already exists. Replace it?")
                    .arg(source.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
                continue;
        }

        QFile input(source.absoluteFilePath());
        QSaveFile output(targetPath);
        if (!input.open(QIODevice::ReadOnly)
            || !output.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(
                this, "Add files",
                QStringLiteral("Could not copy \"%1\".")
                    .arg(source.fileName()));
            continue;
        }

        bool ok = true;
        while (!input.atEnd()) {
            const QByteArray chunk = input.read(1024 * 1024);
            if (chunk.isEmpty() && input.error() != QFile::NoError) {
                ok = false;
                break;
            }
            if (output.write(chunk) != chunk.size()) {
                ok = false;
                break;
            }
        }
        if (!ok || !output.commit()) {
            output.cancelWriting();
            QMessageBox::warning(
                this, "Add files",
                QStringLiteral("Could not copy \"%1\".")
                    .arg(source.fileName()));
            continue;
        }
        ++copied;
    }

    if (copied > 0) {
        m_projectFileSystemModel->refreshDirectory(destinationDirectory);
        updateProjectActions();
        statusBar()->showMessage(
            QStringLiteral("%1 file(s) added").arg(copied), 4000);
    }
    return copied > 0;
}

void MainWindow::onProjectTreeItemClicked(const QModelIndex &index)
{
    if (!m_previewDock || !m_previewDock->isVisible() || !m_project.isValid())
        return;

    const QString filePath = projectPathFromIndex(index);
    const QFileInfo fi(filePath);

    if (!fi.isFile()) {
        m_previewDock->clear();
        return;
    }

    const QString suffix = fi.suffix().toLower();
    const QStringList textExts = {
        "yaml", "yml", "json", "csv", "tsv", "txt", "log", "md"
    };
    const QStringList imgExts = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};

    if (textExts.contains(suffix)) {
        bool truncated = false;
        const QString text = readPreviewTextFile(filePath, &truncated);
        if (!text.isEmpty() || fi.size() == 0) {
            if (suffix == "md" && !truncated)
                m_previewDock->showMarkdown(text);
            else
                m_previewDock->showText(text);
        } else {
            m_previewDock->clear();
        }
    } else if (imgExts.contains(suffix)) {
        QPixmap pix;
        if (suffix == "tif" || suffix == "tiff") {
            QString errorMessage;
            const RasterData raster = TiffReader::load(filePath, &errorMessage);
            if (!raster.isNull()) {
                const QImage image = RasterRenderer().renderAuto(raster);
                if (!image.isNull())
                    pix = QPixmap::fromImage(image);
            }
        } else {
            pix.load(filePath);
        }
        if (!pix.isNull())
            m_previewDock->showImage(pix);
        else
            m_previewDock->clear();
    } else {
        m_previewDock->clear();
    }
}

void MainWindow::onWorkspaceTabCloseRequested(int index)
{
    QWidget *tabWidget = ui->workspaceTabs->widget(index);
    if (!tabWidget)
        return;

    if (m_activeViewer
        && (tabWidget == m_activeViewer || tabWidget->isAncestorOf(m_activeViewer))) {
        m_activeViewer.clear();
        onActiveViewerChanged(nullptr);
    }

    const QString key = m_openImageTabs.key(tabWidget);
    if (!key.isEmpty())
        m_openImageTabs.remove(key);

    ui->workspaceTabs->removeTab(index);
    delete tabWidget;

    // Re-resolve the active viewer from the surviving current tab. This also
    // makes closing a non-current tab harmless.
    m_activeViewer.clear();
    ImageViewer *viewer = currentImageViewer();
    m_activeViewer = viewer;
    onActiveViewerChanged(viewer);

    updateProjectActions();
    updateViewerEmptyState();
    refreshReloadableArtifactWatchers();
}

void MainWindow::onWorkspaceTabContextMenu(const QPoint &position)
{
    if (!ui->workspaceTabs || !ui->workspaceTabs->tabBar())
        return;

    const int tabIndex = ui->workspaceTabs->tabBar()->tabAt(position);
    if (tabIndex < 0)
        return;

    QMenu menu(this);
    QAction *renameTabAction = menu.addAction("Rename tab...");
    menu.addSeparator();
    QAction *closeTabAction = menu.addAction("Close tab");
    QAction *closeOtherTabsAction = menu.addAction("Close other tabs");
    QAction *closeAllTabsAction = menu.addAction("Close all tabs");
    closeOtherTabsAction->setEnabled(ui->workspaceTabs->count() > 1);
    closeAllTabsAction->setEnabled(ui->workspaceTabs->count() > 0);

    QAction *selected = menu.exec(
        ui->workspaceTabs->tabBar()->mapToGlobal(position));
    if (!selected)
        return;

    if (selected == renameTabAction) {
        bool ok = false;
        const QString currentTitle =
            ui->workspaceTabs->tabText(tabIndex);
        const QString title = QInputDialog::getText(
            this,
            "Rename tab",
            "Tab name:",
            QLineEdit::Normal,
            currentTitle,
            &ok).trimmed();
        if (!ok)
            return;
        if (title.isEmpty()) {
            QMessageBox::information(
                this, "Rename tab",
                "Tab name cannot be empty.");
            return;
        }
        ui->workspaceTabs->setTabText(tabIndex, title);
        saveProjectSession();
    } else if (selected == closeTabAction) {
        onWorkspaceTabCloseRequested(tabIndex);
    } else if (selected == closeOtherTabsAction) {
        closeOtherWorkspaceTabs(tabIndex);
    } else if (selected == closeAllTabsAction) {
        closeAllWorkspaceTabs();
    }
}

void MainWindow::closeOtherWorkspaceTabs(int preservedIndex)
{
    if (!ui->workspaceTabs
        || preservedIndex < 0
        || preservedIndex >= ui->workspaceTabs->count()) {
        return;
    }

    QWidget *preservedWidget = ui->workspaceTabs->widget(preservedIndex);
    for (int i = ui->workspaceTabs->count() - 1; i >= 0; --i) {
        if (ui->workspaceTabs->widget(i) == preservedWidget)
            continue;
        onWorkspaceTabCloseRequested(i);
    }

    const int newIndex = ui->workspaceTabs->indexOf(preservedWidget);
    if (newIndex >= 0)
        ui->workspaceTabs->setCurrentIndex(newIndex);
}

void MainWindow::closeAllWorkspaceTabs()
{
    if (!ui->workspaceTabs)
        return;

    for (int i = ui->workspaceTabs->count() - 1; i >= 0; --i)
        onWorkspaceTabCloseRequested(i);
}

void MainWindow::onLayerSelectionChanged(int index)
{
    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    ImageViewer *viewer = currentImageViewer();
    if (!viewer) {
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        updateMaskEditControls(nullptr);
        return;
    }

    LayerManager *mgr = viewer->layerManager();
    if (!mgr) {
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        updateMaskEditControls(viewer);
        return;
    }

    if (index < 0 || index >= mgr->layerCount()) {
        mgr->setActiveLayer(nullptr);
        setHistogramLayer(nullptr);
        ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
        ui->actionEditPoints->setEnabled(viewer->pointEditEnabled());
        PointLayer *editingLayer = viewer->editingPointLayer();
        ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());
        updateMaskEditControls(viewer);
        return;
    }

    Layer *layer = mgr->layerAt(index);
    mgr->setActiveLayer(layer);
    setHistogramLayer(layer);

    if (auto *pointLayer = qobject_cast<PointLayer*>(layer)) {
        m_currentPointLayer = pointLayer;
        m_pointLayerConnection = connect(pointLayer, &PointLayer::selectionChanged,
                                         this, &MainWindow::onPointSelectionChanged);
        m_pointCollectionConnection = connect(
            pointLayer, &PointLayer::collectionChanged,
            this, &MainWindow::onPointSelectionChanged);
        updatePropertiesForPointLayer(pointLayer);
        ui->actionEditPoints->setEnabled(
            !pointLayer->usesAggregateRendering()
            || viewer->pointEditEnabled());
    } else {
        m_currentPointLayer = nullptr;
        ui->actionEditPoints->setEnabled(viewer->pointEditEnabled());
    }

    PointLayer *editingLayer = viewer->editingPointLayer();
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());
    updateMaskEditControls(viewer);
}

void MainWindow::setHistogramLayer(Layer *layer)
{
    disconnect(m_histogramStyleConnection);
    disconnect(m_histogramDataConnection);
    disconnect(m_histogramMaskEditConnection);

    auto *imageLayer = qobject_cast<ImageLayer*>(layer);
    if (m_histogramDock)
        m_histogramDock->setLayer(imageLayer);

    if (!imageLayer || !m_histogramDock)
        return;

    m_histogramStyleConnection =
        connect(imageLayer, &Layer::styleChanged, this,
                [this, imageLayer]() {
                    if (m_histogramDock)
                        m_histogramDock->setLayer(imageLayer);
                });
    m_histogramDataConnection =
        connect(imageLayer, &Layer::dataChanged, this,
                [this, imageLayer]() {
                    if (m_histogramDock)
                        m_histogramDock->setLayer(imageLayer);
                });
    m_histogramMaskEditConnection =
        connect(imageLayer, &ImageLayer::binaryMaskEditModeChanged, this,
                [this, imageLayer]() {
                    if (m_histogramDock)
                        m_histogramDock->setLayer(imageLayer);
                });
}

void MainWindow::onPointSelectionChanged()
{
    if (!m_currentPointLayer || !ui->propertiesTable)
        return;

    const PointCollection &col = m_currentPointLayer->collection();
    const int selectionCount =
        m_currentPointLayer->selectedPointIndices().size();
    const int index = m_currentPointLayer->primarySelectedPointIndex();
    const int sectionARows = 4;
    QVector<QPair<QString, QString>> selectionSummary;
    if (selectionCount > 1) {
        const QStringList colNames = col.columnNames();
        const QSet<int> selectedIndices =
            m_currentPointLayer->selectedPointIndices();
        const int attributeCount = qMax(0, colNames.size() - 2);
        for (int attr = 0; attr < attributeCount; ++attr) {
            int validCount = 0;
            bool allNumeric = true;
            double sum = 0.0;
            double minValue = 0.0;
            double maxValue = 0.0;
            QSet<QString> distinctValues;

            for (int selectedIndex : selectedIndices) {
                if (selectedIndex < 0 || selectedIndex >= col.points().size())
                    continue;

                const QVector<QVariant> &attributes =
                    col.points().at(selectedIndex).attributes;
                if (attr >= attributes.size()
                    || !attributes.at(attr).isValid()) {
                    continue;
                }

                const QVariant value = attributes.at(attr);
                ++validCount;
                bool numericOk = false;
                const double numericValue = value.toDouble(&numericOk);
                if (numericOk) {
                    sum += numericValue;
                    if (validCount == 1) {
                        minValue = numericValue;
                        maxValue = numericValue;
                    } else {
                        minValue = qMin(minValue, numericValue);
                        maxValue = qMax(maxValue, numericValue);
                    }
                } else {
                    allNumeric = false;
                }
                distinctValues.insert(value.toString());
            }

            const QString attrName = (attr + 2 < colNames.size())
                ? colNames.at(attr + 2)
                : QStringLiteral("Attr %1").arg(attr);
            QString summary;
            if (validCount == 0) {
                summary = "all NA";
            } else if (allNumeric) {
                summary = QStringLiteral("mean %1; range %2 - %3")
                              .arg(sum / validCount, 0, 'g', 6)
                              .arg(minValue, 0, 'g', 6)
                              .arg(maxValue, 0, 'g', 6);
            } else {
                summary = QStringLiteral("%1 distinct")
                              .arg(distinctValues.size());
                if (distinctValues.size() <= 3) {
                    QStringList values = distinctValues.values();
                    values.sort(Qt::CaseInsensitive);
                    summary += QStringLiteral(": %1").arg(values.join(", "));
                }
            }
            selectionSummary.append({attrName, summary});
        }
    }
    const int sectionBRows = selectionCount == 1 && index >= 0
        ? 3 + col.points()[index].attributes.size()
        : (selectionCount > 1 ? 1 + selectionSummary.size() : 0);

    ui->propertiesTable->setRowCount(sectionARows + sectionBRows);

    // Section A — File info
    ui->propertiesTable->setItem(0, 0, new QTableWidgetItem("Type"));
    ui->propertiesTable->setItem(0, 1, new QTableWidgetItem("Point Layer"));
    ui->propertiesTable->setItem(1, 0, new QTableWidgetItem("Source"));
    ui->propertiesTable->setItem(1, 1, new QTableWidgetItem(m_currentPointLayer->sourcePath()));
    ui->propertiesTable->setItem(2, 0, new QTableWidgetItem("Records"));
    ui->propertiesTable->setItem(2, 1, new QTableWidgetItem(QString::number(col.recordCount())));
    ui->propertiesTable->setItem(3, 0, new QTableWidgetItem("Columns"));
    ui->propertiesTable->setItem(3, 1, new QTableWidgetItem(col.columnNames().join(", ")));

    // Section B — Selected point detail
    if (selectionCount > 1) {
        int row = sectionARows;
        ui->propertiesTable->setItem(
            row, 0, new QTableWidgetItem("Selected"));
        ui->propertiesTable->setItem(
            row, 1,
            new QTableWidgetItem(QString::number(selectionCount)));
        ++row;
        for (const auto &summary : std::as_const(selectionSummary)) {
            ui->propertiesTable->setItem(
                row, 0, new QTableWidgetItem(summary.first));
            ui->propertiesTable->setItem(
                row, 1, new QTableWidgetItem(summary.second));
            ++row;
        }
    } else if (selectionCount == 1
               && index >= 0 && index < col.points().size()) {
        const PointRecord &rec = col.points()[index];
        int row = sectionARows;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("ID"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(index + 1)));
        ++row;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("Row"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(rec.row)));
        ++row;

        ui->propertiesTable->setItem(row, 0, new QTableWidgetItem("Col"));
        ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(QString::number(rec.col)));
        ++row;

        const QStringList colNames = col.columnNames();
        for (int i = 0; i < rec.attributes.size(); ++i) {
            const int csvColIndex = i + 2;
            const QString attrName = (csvColIndex < colNames.size())
                ? colNames[csvColIndex] : QStringLiteral("Attr %1").arg(i);
            const QString valueStr = rec.attributes[i].isValid()
                ? rec.attributes[i].toString() : "NA";
            ui->propertiesTable->setItem(row, 0, new QTableWidgetItem(attrName));
            ui->propertiesTable->setItem(row, 1, new QTableWidgetItem(valueStr));
            ++row;
        }
    }
}

void MainWindow::onEditPointsToggled(bool enabled)
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    if (enabled) {
        if (ui->actionEditMask->isChecked())
            ui->actionEditMask->setChecked(false);
        if (ui->actionPan->isChecked())
            ui->actionPan->setChecked(false);
        if (ui->actionZoom->isChecked())
            ui->actionZoom->setChecked(false);
    }

    viewer->setPointEditEnabled(enabled);

    // Update Cancel edit button state
    PointLayer *pointLayer = viewer->editingPointLayer();
    ui->actionCancelEdit->setEnabled(pointLayer && pointLayer->editMode());
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    updateMaskEditControls(viewer);
    refreshReloadableArtifactWatchers();
}

void MainWindow::onCancelEditTriggered()
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    PointLayer *pointLayer = viewer->editingPointLayer();
    if (!pointLayer || !pointLayer->editMode())
        return;

    const auto answer = QMessageBox::question(
        this,
        "Cancel edit",
        "Are you sure you want to revert the point layer to its state before editing started?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    pointLayer->cancelEdit();
    viewer->setPointEditEnabled(false);
    ui->actionEditPoints->setChecked(false);
    ui->actionCancelEdit->setEnabled(false);
    updateMaskEditControls(viewer);
    refreshReloadableArtifactWatchers();
}

void MainWindow::onEditMaskToggled(bool enabled)
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    if (enabled) {
        auto *maskLayer = qobject_cast<ImageLayer *>(
            viewer->layerManager() ? viewer->layerManager()->activeLayer()
                                   : nullptr);
        if (maskLayer && maskLayer->binaryMaskNeedsImageJNormalization()) {
            QMessageBox warning(this);
            warning.setIcon(QMessageBox::Warning);
            warning.setWindowTitle("Edit mask");
            warning.setText(
                "This mask is binary, but it is not encoded as 0/255.");
            warning.setInformativeText(
                "rcaiman-projects can edit it, but entering mask edit mode "
                "will rewrite the mask using the 0/255 convention used by "
                "ImageJ and rcaiman.\n\n"
                "This will happen even if you enter and leave edit mode "
                "without painting.");
            QPushButton *continueButton =
                warning.addButton("Continue", QMessageBox::AcceptRole);
            QPushButton *cancelButton =
                warning.addButton(QMessageBox::Cancel);
            warning.setDefaultButton(cancelButton);
            warning.exec();
            if (warning.clickedButton() != continueButton) {
                const QSignalBlocker blocker(ui->actionEditMask);
                ui->actionEditMask->setChecked(false);
                updateMaskEditControls(viewer);
                return;
            }
        }

        if (ui->actionEditPoints->isChecked())
            ui->actionEditPoints->setChecked(false);
        if (ui->actionPan->isChecked())
            ui->actionPan->setChecked(false);
        if (ui->actionZoom->isChecked())
            ui->actionZoom->setChecked(false);
        if (m_maskBrushSpin)
            viewer->setMaskBrushSize(m_maskBrushSpin->value());
    }

    QString errorMessage;
    if (!viewer->setMaskEditEnabled(enabled, &errorMessage)) {
        QMessageBox::warning(
            this, "Edit mask",
            errorMessage.isEmpty()
                ? QStringLiteral("Could not change mask edit mode.")
                : errorMessage);
        const QSignalBlocker blocker(ui->actionEditMask);
        ui->actionEditMask->setChecked(viewer->maskEditEnabled());
    }

    updateMaskEditControls(viewer);
    refreshReloadableArtifactWatchers();
}

void MainWindow::onInvertMaskTriggered()
{
    ImageViewer *viewer = currentImageViewer();
    ImageLayer *layer = viewer ? viewer->editingMaskLayer() : nullptr;
    if (!viewer || !viewer->maskEditEnabled() || !layer)
        return;

    int changedPixels = 0;
    QString errorMessage;
    if (!layer->invertBinaryMask(&changedPixels, &errorMessage)) {
        QMessageBox::warning(
            this, "Invert mask",
            errorMessage.isEmpty()
                ? QStringLiteral("Could not invert the active mask.")
                : errorMessage);
        return;
    }

    if (!layer->saveBinaryMask(&errorMessage)) {
        QMessageBox::warning(
            this, "Invert mask",
            errorMessage.isEmpty()
                ? QStringLiteral("Could not save mask changes.")
                : errorMessage);
        return;
    }

    ui->statusbar->showMessage(
        QStringLiteral("Inverted mask (%1 pixels).").arg(changedPixels),
        3000);
}

void MainWindow::onCancelMaskEditTriggered()
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer || !viewer->maskEditEnabled())
        return;

    const auto answer = QMessageBox::question(
        this,
        "Cancel mask edit",
        "Are you sure you want to revert the mask to its state before editing started?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString errorMessage;
    if (!viewer->cancelMaskEdit(&errorMessage)) {
        QMessageBox::warning(
            this, "Cancel mask edit",
            errorMessage.isEmpty()
                ? QStringLiteral("Could not cancel mask editing.")
                : errorMessage);
        return;
    }

    {
        const QSignalBlocker blocker(ui->actionEditMask);
        ui->actionEditMask->setChecked(false);
    }
    updateMaskEditControls(viewer);
    refreshReloadableArtifactWatchers();
}

void MainWindow::onWorkspaceTabChanged(int index)
{
    // Clean up previous point layer connection
    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    if (index < 0) {
        m_activeViewer = nullptr;
        onActiveViewerChanged(nullptr);
        return;
    }

    // Find the first viewer in the splitter for this tab
    auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(index));
    if (!splitter || splitter->count() == 0) {
        onActiveViewerChanged(nullptr);
        return;
    }

    auto *viewer = qobject_cast<ImageViewer*>(splitter->widget(0));
    m_activeViewer = viewer;
    onActiveViewerChanged(viewer);
}

void MainWindow::onActiveViewerChanged(ImageViewer *viewer)
{
    if (!viewer) {
        {
            const QSignalBlocker panBlocker(ui->actionPan);
            const QSignalBlocker zoomBlocker(ui->actionZoom);
            ui->actionPan->setChecked(false);
            ui->actionZoom->setChecked(false);
        }
        ui->actionPan->setEnabled(false);
        ui->actionZoom->setEnabled(false);
        if (m_zoomPercentSpin) {
            const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
            m_zoomPercentSpin->setEnabled(false);
            m_zoomPercentSpin->setValue(100.0);
        }
        ui->actionSyncSplitViews->setEnabled(false);
        ui->actionSyncSplitZoom->setEnabled(false);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditPoints->setEnabled(false);
        ui->actionCancelEdit->setEnabled(false);
        updateMaskEditControls(nullptr);
        if (m_layersDock)
            m_layersDock->setLayerManager(nullptr);
        setHistogramLayer(nullptr);
        clearPropertiesTable();
        return;
    }

    {
        const QSignalBlocker panBlocker(ui->actionPan);
        const QSignalBlocker zoomBlocker(ui->actionZoom);
        ui->actionPan->setChecked(viewer->panModeEnabled());
        ui->actionZoom->setChecked(viewer->zoomModeEnabled());
    }
    ui->actionPan->setEnabled(true);
    ui->actionZoom->setEnabled(true);
    if (m_zoomPercentSpin) {
        const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
        m_zoomPercentSpin->setEnabled(true);
        m_zoomPercentSpin->setValue(viewer->zoomPercent());
    }

    // Enable sync only if the viewer is inside a QSplitter with siblings
    auto *splitter = qobject_cast<QSplitter*>(viewer->parentWidget());
    const bool hasSplitSiblings = splitter && splitter->count() > 1;
    ui->actionSyncSplitViews->setEnabled(hasSplitSiblings);
    ui->actionSyncSplitZoom->setEnabled(hasSplitSiblings);

    LayerManager *mgr = viewer->layerManager();
    Layer *active = mgr ? mgr->activeLayer() : nullptr;
    auto *pointLayer = qobject_cast<PointLayer*>(active);
    const bool isPointLayer = (pointLayer != nullptr);
    PointLayer *editingLayer = viewer->editingPointLayer();
    const bool canEditActivePointLayer =
        pointLayer && !pointLayer->usesAggregateRendering();
    ui->actionEditPoints->setEnabled(
        canEditActivePointLayer || viewer->pointEditEnabled());
    ui->actionEditPoints->setChecked(viewer->pointEditEnabled());
    ui->actionCancelEdit->setEnabled(editingLayer && editingLayer->editMode());
    updateMaskEditControls(viewer);

    if (m_layersDock)
        m_layersDock->setLayerManager(mgr);
    setHistogramLayer(active);
}

ImageViewer *MainWindow::viewerForLayer(PointLayer *layer) const
{
    if (!layer)
        return nullptr;

    for (int tabIndex = 0;
         tabIndex < ui->workspaceTabs->count();
         ++tabIndex) {
        auto *splitter = qobject_cast<QSplitter*>(
            ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;
        for (int panelIndex = 0;
             panelIndex < splitter->count();
             ++panelIndex) {
            auto *viewer = qobject_cast<ImageViewer*>(
                splitter->widget(panelIndex));
            if (viewer && viewer->layerManager()
                && viewer->layerManager()->indexOf(layer) >= 0)
                return viewer;
        }
    }
    return nullptr;
}

void MainWindow::openAttributeTable(PointLayer *layer)
{
    if (!layer)
        return;

    if (layer->usesAggregateRendering()) {
        QMessageBox::information(
            this,
            "Open table",
            "This point layer is shown in fast preview mode because it has "
            "many points.\n\n"
            "The attribute table, labels, and point editing are disabled to "
            "keep pan and zoom responsive.");
        return;
    }

    if (auto existing = m_attributeTables.value(layer)) {
        existing->showNormal();
        existing->raise();
        existing->activateWindow();
        return;
    }

    ImageViewer *viewer = viewerForLayer(layer);
    if (!viewer)
        return;

    auto *window = new PointAttributeTableWindow(layer, viewer, this);
    m_attributeTables.insert(layer, window);
    connect(window, &QObject::destroyed, this, [this, layer]() {
        m_attributeTables.remove(layer);
    });
    window->show();
    window->raise();
    window->activateWindow();
}

void MainWindow::closeAttributeTables()
{
    const auto windows = m_attributeTables.values();
    m_attributeTables.clear();

    for (const QPointer<PointAttributeTableWindow> &window : windows) {
        if (!window)
            continue;
        auto *rawWindow = window.data();
        rawWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        rawWindow->close();
        delete rawWindow;
    }
}

void MainWindow::splitViewer()
{
    ImageViewer *viewer = currentImageViewer();
    if (!viewer)
        return;

    auto *splitter = qobject_cast<QSplitter*>(viewer->parentWidget());
    if (!splitter)
        return;

    auto *newViewer = new ImageViewer(this);
    newViewer->setFocusPolicy(Qt::StrongFocus);
    configureViewer(newViewer);

    int idx = splitter->indexOf(viewer);
    splitter->insertWidget(idx + 1, newViewer);
    newViewer->restoreView(viewer->viewTransform(), viewer->viewCenter());

    const int totalWidth = splitter->width();
    if (totalWidth > 0) {
        QList<int> sizes;
        sizes.reserve(splitter->count());
        const int baseSize = totalWidth / splitter->count();
        for (int i = 0; i < splitter->count(); ++i)
            sizes.append(baseSize);
        splitter->setSizes(sizes);
    }

    m_activeViewer = newViewer;
    onActiveViewerChanged(newViewer);
    updateProjectActions();
    newViewer->setFocus();
}

void MainWindow::openImageAtPath(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();

    // Check if this image is already open in any viewer across any tab
    for (int t = 0; t < ui->workspaceTabs->count(); ++t) {
        auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(t));
        if (!splitter) continue;
        for (int i = 0; i < splitter->count(); ++i) {
            auto *v = qobject_cast<ImageViewer*>(splitter->widget(i));
            if (v && v->primarySourcePath() == absolutePath) {
                ui->workspaceTabs->setCurrentIndex(t);
                v->setFocus();
                return;
            }
        }
    }

    auto *viewer = new ImageViewer(this);
    if (!loadImageIntoViewer(viewer, absolutePath)) {
        delete viewer;
        return;
    }

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(viewer);

    const QString tabTitle = fi.fileName();
    const int newIndex = ui->workspaceTabs->addTab(splitter, tabTitle);
    ui->workspaceTabs->setCurrentIndex(newIndex);

    m_openImageTabs.insert(absolutePath, splitter);

    if (m_layersDock)
        m_layersDock->setLayerManager(viewer->layerManager());

    configureViewer(viewer);
    m_activeViewer = viewer;
    updateProjectActions();
    updateViewerEmptyState();
}

bool MainWindow::loadImageIntoViewer(ImageViewer *viewer,
                                     const QString &filePath,
                                     bool showWarning,
                                     bool showProgress)
{
    if (!viewer)
        return false;

    const QFileInfo fi(filePath);
    const QString absolutePath = fi.absoluteFilePath();
    const QVector<Collection> collections = m_project.isValid()
        ? Project::discoverCollections(m_project.rootPath())
        : QVector<Collection>{};
    CollectionAutoloadResolver autoloadResolver;
    QVector<AutoloadCandidate> candidates =
        autoloadResolver.resolve(
            m_project.isValid() ? m_project.rootPath() : fi.absolutePath(),
            absolutePath,
            collections);

    bool openedFileIncluded = false;
    for (const AutoloadCandidate &candidate : std::as_const(candidates)) {
        if (candidate.openedFile) {
            openedFileIncluded = true;
            break;
        }
    }

    if (!openedFileIncluded) {
        AutoloadCandidate fallback;
        fallback.collection.dirPath = fi.absolutePath();
        fallback.collection.dirName = fi.dir().dirName();
        fallback.filePath = absolutePath;
        fallback.layerKind = AutoloadLayerKind::Image;
        fallback.priority = 100;
        fallback.openedFile = true;
        candidates.prepend(fallback);
    }

    CollectionStyleResolver styleResolver;
    int loadedImageCount = 0;
    const bool progressEnabled = showProgress && !candidates.isEmpty();
    if (progressEnabled) {
        beginStatusProgress(
            QStringLiteral("Opening TIFF: %1").arg(fi.fileName()),
            candidates.size());
    }

    int candidateIndex = 0;
    for (const AutoloadCandidate &candidate : std::as_const(candidates)) {
        ++candidateIndex;
        if (progressEnabled) {
            updateStatusProgress(
                QStringLiteral("Loading layer %1/%2: %3")
                    .arg(candidateIndex)
                    .arg(candidates.size())
                    .arg(QFileInfo(candidate.filePath).fileName()),
                candidateIndex - 1);
        }

        Layer *layer = nullptr;
        if (candidate.layerKind == AutoloadLayerKind::Image) {
            auto *imageLayer = new ImageLayer(candidate.filePath);
            if (imageLayer->rasterData().isNull()) {
                qWarning() << "Autoload: could not load image" << candidate.filePath;
                delete imageLayer;
                continue;
            }
            imageLayer->setStyle(styleResolver.imageStyle(
                candidate.collection, imageLayer->rasterData()));
            layer = imageLayer;
            ++loadedImageCount;
        } else {
            auto *pointLayer = new PointLayer(candidate.filePath, viewer->scene());
            if (!pointLayer->collection().isValid()) {
                qWarning() << "Autoload: could not load points" << candidate.filePath;
                delete pointLayer;
                continue;
            }
            pointLayer->setStyle(styleResolver.pointStyle(
                candidate.collection, pointLayer->collection().recordCount()));
            layer = pointLayer;
        }

        if (!candidate.collection.dirName.isEmpty()) {
            layer->rename(QStringLiteral("%1/%2")
                              .arg(candidate.collection.dirName,
                                   QFileInfo(candidate.filePath).fileName()));
        }
        viewer->layerManager()->addLayer(layer);

        if (progressEnabled)
            updateStatusProgress(
                QStringLiteral("Loaded layer %1/%2")
                    .arg(candidateIndex)
                    .arg(candidates.size()),
                candidateIndex);
    }

    if (loadedImageCount == 0) {
        if (progressEnabled)
            endStatusProgress(QStringLiteral("Could not load TIFF"), 3000);
        if (showWarning) {
            QMessageBox::warning(
                this, "Open Image",
                QStringLiteral("Could not load image:\n%1").arg(absolutePath));
        }
        return false;
    }

    viewer->fitLoadedImage();
    viewer->setPrimarySourcePath(absolutePath);
    qInfo() << "Autoload loaded" << viewer->layerManager()->layerCount()
            << "layer(s) for" << fi.completeBaseName();
    if (progressEnabled) {
        endStatusProgress(
            QStringLiteral("Opened %1 layer%2")
                .arg(viewer->layerManager()->layerCount())
                .arg(viewer->layerManager()->layerCount() == 1 ? "" : "s"),
            3000);
    }
    return true;
}

void MainWindow::configureViewer(ImageViewer *viewer)
{
    if (!viewer)
        return;

    connect(viewer, &ImageViewer::pixelInfoChanged,
            this, &MainWindow::updatePropertiesTable);
    connect(viewer, &ImageViewer::zoomPercentChanged,
            this, [this, viewer](double percent) {
                if (viewer != currentImageViewer() || !m_zoomPercentSpin)
                    return;
                const QSignalBlocker blocker(m_zoomPercentSpin);
                m_zoomPercentSpin->setValue(percent);
            });
    connect(viewer, &ImageViewer::maskBrushSizeChanged,
            this, [this, viewer](int) {
                if (viewer == currentImageViewer())
                    updateMaskBrushControls(viewer);
            });
    connect(viewer, &ImageViewer::maskConnectedRegionInverted,
            this, [this](int pixelCount) {
                statusBar()->showMessage(
                    QStringLiteral("Inverted %1 mask pixel%2")
                        .arg(pixelCount)
                        .arg(pixelCount == 1 ? "" : "s"),
                    4000);
            });
    connect(viewer->layerManager(), &LayerManager::layersChanged,
            this, &MainWindow::refreshReloadableArtifactWatchers);
    connect(viewer, &ImageViewer::activatedByClick, this, [this, viewer]() {
        if (m_activeViewer == viewer)
            return;

        m_activeViewer = viewer;
        onActiveViewerChanged(viewer);
    });
    connect(viewer, &ImageViewer::pointLayerFileDropped,
            this, [this, viewer](const QString &path) {
                if (!viewer || !viewer->layerManager())
                    return;
                if (PointLayer *layer = createStyledPointLayer(path, viewer))
                    viewer->layerManager()->addLayer(layer);
            });
    connect(viewer, &ImageViewer::imageLayerFileDropped,
            this, [this, viewer](const QString &path) {
                if (!viewer || !viewer->layerManager())
                    return;
                auto *layer = createStyledImageLayer(path);
                if (!layer)
                    return;
                viewer->layerManager()->addLayer(layer);
                if (viewer->primarySourcePath().isEmpty())
                    viewer->setPrimarySourcePath(
                        QFileInfo(path).absoluteFilePath());
            });
    connect(viewer, &ImageViewer::closeRequested, this, [this, viewer]() {
        auto *v = qobject_cast<ImageViewer*>(sender());
        if (!v || v != viewer)
            return;

        auto *splitter = qobject_cast<QSplitter*>(v->parentWidget());
        if (!splitter)
            return;

        if (splitter->count() <= 1) {
            QPointer<QWidget> tabWidget = splitter;
            for (int t = 0; t < ui->workspaceTabs->count(); ++t) {
                if (ui->workspaceTabs->widget(t) == splitter) {
                    // The request originates inside ImageViewer::contextMenuEvent().
                    // Closing synchronously would destroy that viewer before its
                    // event handler has returned.
                    QTimer::singleShot(0, this, [this, tabWidget]() {
                        if (!tabWidget)
                            return;

                        const int tabIndex = ui->workspaceTabs->indexOf(tabWidget);
                        if (tabIndex >= 0)
                            onWorkspaceTabCloseRequested(tabIndex);
                    });
                    return;
                }
            }
            return;
        }

        v->hide();
        v->setParent(nullptr);
        v->deleteLater();
        refreshReloadableArtifactWatchers();

        if (m_activeViewer == v) {
            auto *remainingViewer = qobject_cast<ImageViewer*>(splitter->widget(0));
            m_activeViewer = remainingViewer;
            onActiveViewerChanged(remainingViewer);
            if (remainingViewer)
                remainingViewer->setFocus();
        }
    });
    connect(viewer, &ImageViewer::reloadRequested,
            this, [this, viewer]() {
                auto *v = qobject_cast<ImageViewer*>(sender());
                if (!v || v != viewer)
                    return;
                reloadViewerLayers(v);
            });
    refreshReloadableArtifactWatchers();
}

QModelIndex MainWindow::projectSourceIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    if (index.model() == m_projectFileSystemModel)
        return index;
    if (index.model() == m_projectSortProxy)
        return m_projectSortProxy->mapToSource(index);
    return {};
}

QModelIndex MainWindow::projectProxyIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    if (index.model() == m_projectSortProxy)
        return index;
    if (index.model() == m_projectFileSystemModel)
        return m_projectSortProxy->mapFromSource(index);
    return {};
}

QString MainWindow::projectPathFromIndex(const QModelIndex &index) const
{
    const QModelIndex sourceIndex = projectSourceIndex(index);
    return sourceIndex.isValid()
        ? m_projectFileSystemModel->filePath(sourceIndex)
        : QString();
}

QList<Layer*> MainWindow::reloadableLayersForPath(const QString &path) const
{
    const QString target = normalizedArtifactPath(path);
    QList<Layer*> result;
    if (target.isEmpty())
        return result;

    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || !viewer->layerManager())
                continue;

            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer || !layer->supportsReloadFromDisk())
                    continue;
                if (normalizedArtifactPath(layer->sourcePath()) == target)
                    result.append(layer);
            }
        }
    }

    return result;
}

void MainWindow::reloadViewerLayers(ImageViewer *viewer)
{
    if (!viewer || !viewer->layerManager())
        return;

    LayerManager *manager = viewer->layerManager();
    int attempted = 0;
    int reloaded = 0;
    QStringList failures;

    for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
        Layer *layer = manager->layerAt(layerIndex);
        if (!layer || !layer->supportsReloadFromDisk())
            continue;

        ++attempted;
        QString errorMessage;
        if (layer->reloadFromDisk(&errorMessage)) {
            ++reloaded;
        } else {
            failures.append(QStringLiteral("%1: %2")
                                .arg(QFileInfo(layer->sourcePath()).fileName(),
                                     errorMessage));
        }
    }

    if (attempted == 0) {
        statusBar()->showMessage("No reloadable layers in this panel", 3000);
        return;
    }

    if (QGraphicsScene *scene = viewer->scene()) {
        const QRectF bounds = scene->itemsBoundingRect();
        scene->setSceneRect(bounds);
    }

    refreshReloadableArtifactWatchers();
    if (m_activeViewer == viewer)
        onActiveViewerChanged(viewer);

    if (failures.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("Reloaded %1 layer(s)").arg(reloaded), 3000);
    } else {
        statusBar()->showMessage(
            QStringLiteral("Reloaded %1 of %2 layer(s); %3 failed")
                .arg(reloaded)
                .arg(attempted)
                .arg(failures.size()),
            5000);
        qWarning() << "Manual panel reload failures:" << failures;
    }
}

void MainWindow::refreshReloadableArtifactWatchers()
{
    if (!m_artifactWatcher)
        return;

    QHash<QString, QString> desiredPaths;
    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || !viewer->layerManager())
                continue;

            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0; layerIndex < manager->layerCount(); ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer || !layer->supportsReloadFromDisk())
                    continue;

                const QString path = QFileInfo(layer->sourcePath()).absoluteFilePath();
                if (QFileInfo::exists(path))
                    desiredPaths.insert(normalizedArtifactPath(path), path);
            }
        }
    }

    const QStringList watched = m_artifactWatcher->files();
    for (const QString &path : watched) {
        if (!desiredPaths.contains(normalizedArtifactPath(path)))
            m_artifactWatcher->removePath(path);
    }

    QSet<QString> watchedKeys;
    for (const QString &path : m_artifactWatcher->files())
        watchedKeys.insert(normalizedArtifactPath(path));

    for (auto it = desiredPaths.cbegin(); it != desiredPaths.cend(); ++it) {
        if (!watchedKeys.contains(it.key()))
            m_artifactWatcher->addPath(it.value());
    }
}

void MainWindow::scheduleArtifactReload(const QString &path, int delayMs)
{
    const QString key = normalizedArtifactPath(path);
    if (key.isEmpty())
        return;

    if (!m_pendingReloadAttempts.contains(key))
        m_pendingReloadAttempts.insert(key, 0);

    if (m_scheduledArtifactReloads.contains(key))
        return;
    m_scheduledArtifactReloads.insert(key);

    QTimer::singleShot(delayMs, this, [this, key]() {
        m_scheduledArtifactReloads.remove(key);
        reloadArtifactFromDisk(key);
    });
}

void MainWindow::reloadArtifactFromDisk(const QString &path)
{
    const QString key = normalizedArtifactPath(path);
    if (key.isEmpty())
        return;

    QList<Layer*> layers = reloadableLayersForPath(key);
    if (layers.isEmpty()) {
        m_pendingReloadAttempts.remove(key);
        refreshReloadableArtifactWatchers();
        return;
    }

    const QString sourcePath = layers.first()->sourcePath();
    const QFileInfo info(sourcePath);
    const bool pointsArtifact = qobject_cast<PointLayer*>(layers.first()) != nullptr;
    const QString artifactKind = pointsArtifact
        ? QStringLiteral("points")
        : QStringLiteral("image");
    QString errorMessage;
    bool ok = info.exists() && info.isFile();
    if (ok) {
        for (Layer *layer : std::as_const(layers)) {
            if (!layer->reloadFromDisk(&errorMessage)) {
                ok = false;
                break;
            }
        }
    } else {
        errorMessage = QStringLiteral("File is not available yet.");
    }

    if (!ok) {
        const int attempts = m_pendingReloadAttempts.value(key, 0) + 1;
        m_pendingReloadAttempts.insert(key, attempts);
        if (attempts <= 8) {
            statusBar()->showMessage(
                QStringLiteral("Waiting for stable %1: %2")
                    .arg(artifactKind, info.fileName()),
                1500);
            scheduleArtifactReload(sourcePath, 500);
            refreshReloadableArtifactWatchers();
            return;
        }

        m_pendingReloadAttempts.remove(key);
        statusBar()->showMessage(
            QStringLiteral("Could not reload %1: %2")
                .arg(artifactKind, info.fileName()),
            5000);
        qWarning() << "Hot reload failed for" << sourcePath << errorMessage;
        refreshReloadableArtifactWatchers();
        return;
    }

    m_pendingReloadAttempts.remove(key);
    refreshReloadableArtifactWatchers();
    statusBar()->showMessage(
        QStringLiteral("Reloaded %1: %2").arg(artifactKind, info.fileName()),
        3000);

    if (m_activeViewer)
        onActiveViewerChanged(m_activeViewer);
}

void MainWindow::loadProject(const Project &project)
{
    // Only the folder containing project.yaml identifies a project. Its parent
    // folder may contain many sibling projects and must not be treated as open.
    if (m_project.isValid()
        && sameProjectRoot(m_project.rootPath(), project.rootPath())) {
        m_project = project;
        updateProjectExplorer();
        updateProjectActions();
        setWindowTitle(QString("%1 - rcaiman-projects").arg(m_project.name()));
        if (m_layersDock)
            m_layersDock->setProjectPath(m_project.rootPath());
        if (m_previewDock)
            m_previewDock->show();
        addToRecentProjects(m_project.rootPath());
        updateViewerEmptyState();
        statusBar()->showMessage("Project is already open", 3000);
        return;
    }

    struct ProjectLoadFeedback {
        MainWindow *window = nullptr;
        QProgressBar *progress = nullptr;
        bool finished = false;

        ProjectLoadFeedback(MainWindow *mainWindow, const QString &projectName)
            : window(mainWindow)
            , progress(new QProgressBar(mainWindow->statusBar()))
        {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            progress->setMinimum(0);
            progress->setMaximum(0);
            progress->setTextVisible(false);
            progress->setMinimumWidth(160);
            window->statusBar()->addPermanentWidget(progress);
            setStage(QStringLiteral("Opening project: %1...")
                         .arg(projectName));
        }

        ~ProjectLoadFeedback()
        {
            if (progress) {
                window->statusBar()->removeWidget(progress);
                progress->deleteLater();
            }
            QApplication::restoreOverrideCursor();
            if (!finished)
                window->statusBar()->showMessage(
                    QStringLiteral("Could not open project"), 5000);
        }

        void setStage(const QString &message)
        {
            window->statusBar()->showMessage(message, 0);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        void complete(const QString &projectName)
        {
            finished = true;
            window->statusBar()->showMessage(
                QStringLiteral("Project ready: %1").arg(projectName), 5000);
        }
    };

    const QString projectName =
        project.name().isEmpty()
            ? QFileInfo(project.rootPath()).fileName()
            : project.name();
    ProjectLoadFeedback feedback(this, projectName);

    feedback.setStage(QStringLiteral("Closing current project..."));
    closeCurrentProject();

    feedback.setStage(QStringLiteral("Loading project structure..."));
    m_project = project;
    m_projectExplorerGroupExpansion.clear();
    loadProjectFileMarks();
    loadProjectExplorerState();
    updateProjectExplorer();
    updateProjectActions();
    setWindowTitle(QString("%1 - rcaiman-projects").arg(m_project.name()));
    if (m_layersDock)
        m_layersDock->setProjectPath(m_project.rootPath());
    if (m_previewDock)
        m_previewDock->show();
    addToRecentProjects(m_project.rootPath());

    feedback.setStage(QStringLiteral("Restoring session..."));
    restoreProjectSession();
    updateViewerEmptyState();

    feedback.complete(projectName);
}

void MainWindow::closeCurrentProject()
{
    if (m_collectionGraphDialog) {
        m_collectionGraphDialog->close();
        m_collectionGraphDialog = nullptr;
    }

    saveProjectSession();
    saveProjectFileMarks();
    saveProjectExplorerState();

    closeAttributeTables();

    m_pendingReloadAttempts.clear();
    m_scheduledArtifactReloads.clear();
    if (m_artifactWatcher)
        m_artifactWatcher->removePaths(m_artifactWatcher->files());

    if (m_currentPointLayer) {
        disconnect(m_pointLayerConnection);
        disconnect(m_pointCollectionConnection);
        m_currentPointLayer = nullptr;
    }

    setHistogramLayer(nullptr);
    if (m_layersDock) {
        m_layersDock->setLayerManager(nullptr);
        m_layersDock->setProjectPath(QString());
    }
    if (m_previewDock) {
        m_previewDock->clear();
        m_previewDock->hide();
    }
    clearPropertiesTable();

    {
        const QSignalBlocker panBlocker(ui->actionPan);
        const QSignalBlocker zoomBlocker(ui->actionZoom);
        const QSignalBlocker editBlocker(ui->actionEditPoints);
        const QSignalBlocker maskEditBlocker(ui->actionEditMask);
        ui->actionPan->setChecked(false);
        ui->actionZoom->setChecked(false);
        ui->actionEditPoints->setChecked(false);
        ui->actionEditMask->setChecked(false);
    }
    ui->actionPan->setEnabled(false);
    ui->actionZoom->setEnabled(false);
    if (m_zoomPercentSpin) {
        const QSignalBlocker zoomSpinBlocker(m_zoomPercentSpin);
        m_zoomPercentSpin->setEnabled(false);
        m_zoomPercentSpin->setValue(100.0);
    }
    ui->actionSyncSplitViews->setEnabled(false);
    ui->actionSyncSplitZoom->setEnabled(false);
    ui->actionEditPoints->setEnabled(false);
    ui->actionCancelEdit->setEnabled(false);
    ui->actionEditMask->setEnabled(false);
    ui->actionCancelMaskEdit->setEnabled(false);
    if (m_maskBrushSpin)
        m_maskBrushSpin->setEnabled(false);
    if (m_maskBrushSlider)
        m_maskBrushSlider->setEnabled(false);

    m_activeViewer.clear();

    {
        const QSignalBlocker blocker(ui->workspaceTabs);
        while (ui->workspaceTabs->count() > 0) {
            QWidget *tab = ui->workspaceTabs->widget(0);
            ui->workspaceTabs->removeTab(0);
            delete tab;
        }
    }

    m_openImageTabs.clear();

    m_project = Project();
    m_projectFileMarks.clear();
    m_projectExplorerGroupExpansion.clear();
    if (m_starredOnlyCheck) {
        const QSignalBlocker blocker(m_starredOnlyCheck);
        m_starredOnlyCheck->setChecked(false);
    }
    if (m_projectSortProxy)
        static_cast<ProjectExplorerSortProxyModel*>(
            m_projectSortProxy)->setStarredOnly(false);
    m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
    resetProjectExplorerToFileSystemRoot();
    updateProjectHeader();
    setWindowTitle("rcaiman-projects");
    updateProjectActions();
    updateViewerEmptyState();
}

void MainWindow::beginStatusProgress(const QString &message, int maximum)
{
    if (!m_statusProgressBar) {
        m_statusProgressBar = new QProgressBar(statusBar());
        m_statusProgressBar->setMinimumWidth(160);
        m_statusProgressBar->setMaximumWidth(220);
        statusBar()->addPermanentWidget(m_statusProgressBar);
    }

    m_statusProgressBar->setMinimum(0);
    m_statusProgressBar->setMaximum(maximum > 0 ? maximum : 0);
    m_statusProgressBar->setValue(0);
    m_statusProgressBar->setTextVisible(maximum > 0);
    if (maximum > 0)
        m_statusProgressBar->setFormat(QStringLiteral("%v/%m"));
    else
        m_statusProgressBar->setFormat(QString());
    statusBar()->showMessage(message, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::updateStatusProgress(const QString &message, int value)
{
    if (!m_statusProgressBar)
        return;

    if (value >= 0)
        m_statusProgressBar->setValue(value);
    if (!message.isEmpty())
        statusBar()->showMessage(message, 0);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::endStatusProgress(const QString &message, int timeoutMs)
{
    if (!m_statusProgressBar)
        return;

    statusBar()->removeWidget(m_statusProgressBar);
    m_statusProgressBar->deleteLater();
    m_statusProgressBar = nullptr;
    if (!message.isEmpty())
        statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::saveApplicationState()
{
    AppPreferences::setMainWindowGeometry(saveGeometry());
    AppPreferences::setMainWindowState(saveState(1));
}

void MainWindow::restoreApplicationState()
{
    const QByteArray geometry = AppPreferences::mainWindowGeometry();
    const QByteArray state = AppPreferences::mainWindowState();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    if (!state.isEmpty())
        restoreState(state, 1);
}

void MainWindow::resetWindowLayout()
{
    if (m_resetLayoutPending)
        return;

    m_resetLayoutPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_resetLayoutPending = false;

        if (m_defaultWindowState.isEmpty()
            || !restoreState(m_defaultWindowState, 1)) {
            statusBar()->showMessage(
                "Window layout could not be reset", 4000);
            return;
        }

        if (m_previewDock)
            m_previewDock->setVisible(m_project.isValid());

        AppPreferences::setMainWindowState(m_defaultWindowState);
        statusBar()->showMessage("Window layout reset", 3000);
    });
}

QString MainWindow::projectRelativePath(const QString &absolutePath) const
{
    if (!m_project.isValid() || absolutePath.isEmpty())
        return {};
    return QDir::fromNativeSeparators(
        QDir(m_project.rootPath()).relativeFilePath(absolutePath));
}

QString MainWindow::projectAbsolutePath(const QString &relativePath) const
{
    if (!m_project.isValid() || relativePath.isEmpty())
        return {};
    return QFileInfo(
        QDir(m_project.rootPath()).filePath(relativePath)).absoluteFilePath();
}

QString MainWindow::projectFileMarksPath() const
{
    if (!m_project.isValid())
        return {};
    return QDir(m_project.rootPath()).filePath(".rcaiman/file_marks.json");
}

QString MainWindow::projectExplorerStatePath() const
{
    if (!m_project.isValid())
        return {};
    return QDir(m_project.rootPath()).filePath(
        ".rcaiman/project_explorer_state.json");
}

void MainWindow::loadProjectFileMarks()
{
    m_projectFileMarks.clear();
    if (!m_project.isValid()) {
        if (m_projectFileSystemModel)
            m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        return;
    }

    QFile file(projectFileMarksPath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (m_projectFileSystemModel)
            m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        if (m_projectFileSystemModel)
            m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        return;
    }

    const QJsonValue marksValue = document.object().value("marks");
    if (marksValue.isArray()) {
        const QJsonArray marks = marksValue.toArray();
        for (const QJsonValue &value : marks) {
            const QString relativePath =
                QDir::fromNativeSeparators(value.toString());
            if (relativePath.isEmpty())
                continue;
            const QString absolutePath = projectAbsolutePath(relativePath);
            if (!QFileInfo::exists(absolutePath))
                continue;
            m_projectFileMarks.insert(relativePath);
        }
    } else if (marksValue.isObject()) {
        const QJsonObject marks = marksValue.toObject();
        for (auto it = marks.constBegin(); it != marks.constEnd(); ++it) {
            const QString relativePath = QDir::fromNativeSeparators(it.key());
            if (relativePath.isEmpty())
                continue;
            if (it.value().isBool() && !it.value().toBool())
                continue;
            if (it.value().isNull() || it.value().isUndefined())
                continue;
            const QString absolutePath = projectAbsolutePath(relativePath);
            if (!QFileInfo::exists(absolutePath))
                continue;
            m_projectFileMarks.insert(relativePath);
        }
    } else {
        if (m_projectFileSystemModel)
            m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        return;
    }

    if (m_projectFileSystemModel)
        m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
}

void MainWindow::loadProjectExplorerState()
{
    bool starredOnly = false;
    if (m_project.isValid()) {
        QFile file(projectExplorerStatePath());
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError
                && document.isObject()) {
                starredOnly =
                    document.object().value("starred_only").toBool(false);
            }
        }
    }

    if (m_starredOnlyCheck) {
        const QSignalBlocker blocker(m_starredOnlyCheck);
        m_starredOnlyCheck->setChecked(starredOnly);
    }
    if (m_projectSortProxy)
        static_cast<ProjectExplorerSortProxyModel*>(
            m_projectSortProxy)->setStarredOnly(starredOnly);
    updateProjectExplorerStarredEmptyMessage();
}

void MainWindow::saveProjectExplorerState() const
{
    if (!m_project.isValid() || !m_starredOnlyCheck)
        return;

    const QString statePath = projectExplorerStatePath();
    if (!m_starredOnlyCheck->isChecked()) {
        QFile::remove(statePath);
        return;
    }

    const QString stateDirectory =
        QDir(m_project.rootPath()).filePath(".rcaiman");
    if (!QDir().mkpath(stateDirectory))
        return;

    QJsonObject root;
    root["schema_version"] = 1;
    root["starred_only"] = true;

    QSaveFile file(statePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void MainWindow::saveProjectFileMarks() const
{
    if (!m_project.isValid())
        return;

    const QString marksPath = projectFileMarksPath();
    if (m_projectFileMarks.isEmpty()) {
        QFile::remove(marksPath);
        return;
    }

    const QString sessionDirectory =
        QDir(m_project.rootPath()).filePath(".rcaiman");
    if (!QDir().mkpath(sessionDirectory))
        return;

    QStringList paths = m_projectFileMarks.values();
    paths.sort(Qt::CaseInsensitive);
    QJsonArray marks;
    for (const QString &relativePath : paths)
        marks.append(relativePath);

    QJsonObject root;
    root["schema_version"] = 2;
    root["marks"] = marks;

    QSaveFile file(marksPath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void MainWindow::clearProjectFileMarks()
{
    if (!m_project.isValid() || m_projectFileMarks.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this,
        "Unstar all",
        "Remove all stars from this project?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer != QMessageBox::Yes)
        return;

    m_projectFileMarks.clear();
    saveProjectFileMarks();
    if (m_projectFileSystemModel)
        m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
    const bool filteringStarred =
        m_starredOnlyCheck && m_starredOnlyCheck->isChecked();
    if (filteringStarred && m_projectSortProxy) {
        static_cast<ProjectExplorerSortProxyModel*>(
            m_projectSortProxy)->invalidate();
        refreshProjectExplorerRootIndex();
        expandProjectExplorerGroups();
    } else if (ui->projectTree && ui->projectTree->viewport()) {
        ui->projectTree->viewport()->update();
    }
    updateProjectExplorerStarredEmptyMessage();
    updateProjectActions();
    statusBar()->showMessage("Removed all stars", 3000);
}

bool MainWindow::projectFileMarkHitTest(
    const QPoint &position,
    QModelIndex *rowIndex) const
{
    if (!ui->projectTree || !m_project.isValid())
        return false;

    const QModelIndex index = ui->projectTree->indexAt(position);
    if (!index.isValid())
        return false;

    const QModelIndex markIndex = index.sibling(index.row(), 0);
    if (markIndex.data(ProjectExplorerGroupRole).toBool())
        return false;

    const QRect rowRect = ui->projectTree->visualRect(markIndex);
    if (!projectFileMarkAreaRect(rowRect).contains(position))
        return false;

    if (rowIndex)
        *rowIndex = markIndex;
    return true;
}

void MainWindow::toggleProjectFileMark(const QString &path)
{
    if (!m_project.isValid() || path.isEmpty())
        return;

    const QFileInfo info(path);
    if (!info.exists())
        return;

    const QString relativePath =
        projectRelativePath(info.absoluteFilePath());
    if (relativePath.isEmpty()
        || relativePath == "."
        || relativePath.startsWith("../")) {
        return;
    }

    const bool marked = m_projectFileMarks.contains(relativePath);
    if (marked)
        m_projectFileMarks.remove(relativePath);
    else
        m_projectFileMarks.insert(relativePath);

    saveProjectFileMarks();
    if (m_projectFileSystemModel) {
        const bool filteringStarred =
            m_starredOnlyCheck && m_starredOnlyCheck->isChecked();
        m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        if (filteringStarred && m_projectSortProxy) {
            static_cast<ProjectExplorerSortProxyModel*>(
                m_projectSortProxy)->invalidate();
            refreshProjectExplorerRootIndex();
            expandProjectExplorerGroups();
        } else if (ui->projectTree && ui->projectTree->viewport()) {
            ui->projectTree->viewport()->update();
        }
        updateProjectExplorerStarredEmptyMessage();
    }
    updateProjectActions();
    statusBar()->showMessage(
        marked ? "Mark cleared" : "Marked",
        2000);
}

void MainWindow::saveProjectSession()
{
    if (!m_project.isValid())
        return;

    QJsonArray tabs;
    for (int tabIndex = 0; tabIndex < ui->workspaceTabs->count(); ++tabIndex) {
        auto *splitter =
            qobject_cast<QSplitter*>(ui->workspaceTabs->widget(tabIndex));
        if (!splitter)
            continue;

        QJsonArray panels;
        int activePanel = 0;
        for (int panelIndex = 0; panelIndex < splitter->count(); ++panelIndex) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(panelIndex));
            if (!viewer || viewer->primarySourcePath().isEmpty())
                continue;

            const QString source = projectRelativePath(
                viewer->primarySourcePath());
            if (source.isEmpty())
                continue;

            const QTransform transform = viewer->viewTransform();
            QJsonArray transformValues{
                transform.m11(), transform.m12(), transform.m13(),
                transform.m21(), transform.m22(), transform.m23(),
                transform.m31(), transform.m32(), transform.m33()
            };
            const QPointF center = viewer->viewCenter();

            QJsonObject panel;
            panel["source"] = source;
            panel["transform"] = transformValues;
            panel["center"] = QJsonArray{center.x(), center.y()};

            QJsonArray layers;
            LayerManager *manager = viewer->layerManager();
            for (int layerIndex = 0;
                 manager && layerIndex < manager->layerCount();
                 ++layerIndex) {
                Layer *layer = manager->layerAt(layerIndex);
                if (!layer)
                    continue;

                const QString layerSource =
                    projectRelativePath(layer->sourcePath());
                if (layerSource.isEmpty())
                    continue;

                QJsonObject layerObject;
                layerObject["source"] = layerSource;
                layerObject["name"] = layer->name();
                layerObject["visible"] = layer->isVisible();
                if (auto *imageLayer = qobject_cast<ImageLayer*>(layer)) {
                    layerObject["type"] = "image";
                    layerObject["image_style"] =
                        imageStyleToJson(imageLayer->style());
                } else if (auto *pointLayer =
                               qobject_cast<PointLayer*>(layer)) {
                    layerObject["type"] = "points";
                    layerObject["point_style"] =
                        pointStyleToJson(pointLayer->style());
                }
                layers.append(layerObject);
            }
            panel["layers"] = layers;
            panels.append(panel);

            if (viewer == m_activeViewer)
                activePanel = panels.size() - 1;
        }

        if (panels.isEmpty())
            continue;

        QJsonArray sizes;
        for (int size : splitter->sizes())
            sizes.append(size);

        QJsonObject tab;
        tab["title"] = ui->workspaceTabs->tabText(tabIndex);
        tab["panels"] = panels;
        tab["split_sizes"] = sizes;
        tab["active_panel"] = activePanel;
        tabs.append(tab);
    }

    QJsonObject session;
    session["schema_version"] = 2;
    session["current_tab"] = ui->workspaceTabs->currentIndex();
    session["sync_split_views"] = ImageViewer::syncEnabled();
    session["sync_split_zoom"] = ImageViewer::syncZoomEnabled();
    session["tabs"] = tabs;

    const QString sessionDirectory =
        QDir(m_project.rootPath()).filePath(".rcaiman");
    if (!QDir().mkpath(sessionDirectory))
        return;

    QSaveFile file(QDir(sessionDirectory).filePath("session.json"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(session).toJson(QJsonDocument::Indented));
    file.commit();
}

void MainWindow::restoreProjectSession()
{
    if (!m_project.isValid())
        return;

    QFile file(QDir(m_project.rootPath()).filePath(".rcaiman/session.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        qWarning() << "Session restore skipped:" << parseError.errorString();
        return;
    }

    const QJsonObject session = document.object();
    const int schemaVersion =
        session.value("schema_version").toInt();
    if (schemaVersion < 1 || schemaVersion > 2)
        return;

    const QJsonArray tabs = session.value("tabs").toArray();
    int restoredCurrentTab = session.value("current_tab").toInt(0);
    ImageViewer *requestedActiveViewer = nullptr;

    for (const QJsonValue &tabValue : tabs) {
        const QJsonObject tabObject = tabValue.toObject();
        const QJsonArray panels = tabObject.value("panels").toArray();
        auto *splitter = new QSplitter(Qt::Horizontal);
        int restoredPanels = 0;
        const int activePanel =
            tabObject.value("active_panel").toInt(0);

        for (int panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
            const QJsonObject panelObject = panels.at(panelIndex).toObject();
            const QString absolutePath = projectAbsolutePath(
                panelObject.value("source").toString());
            if (!QFileInfo::exists(absolutePath))
                continue;

            auto *viewer = new ImageViewer(this);
            const QJsonArray savedLayers =
                panelObject.value("layers").toArray();
            LayerManager *manager = viewer->layerManager();
            int loadedImageCount = 0;

            // LayerManager inserts at the top. Restoring in reverse preserves
            // the exact top-to-bottom order recorded in the session.
            for (int layerIndex = savedLayers.size() - 1;
                 layerIndex >= 0;
                 --layerIndex) {
                const QJsonObject layerObject =
                    savedLayers.at(layerIndex).toObject();
                const QString sourcePath = projectAbsolutePath(
                    layerObject.value("source").toString());
                if (!QFileInfo::exists(sourcePath))
                    continue;

                QString type = layerObject.value("type").toString();
                if (type.isEmpty()) {
                    type = QFileInfo(sourcePath).suffix().compare(
                               "csv", Qt::CaseInsensitive) == 0
                        ? QStringLiteral("points")
                        : QStringLiteral("image");
                }

                Layer *layer = nullptr;
                if (type == QStringLiteral("points")) {
                    auto *pointLayer =
                        new PointLayer(sourcePath, viewer->scene());
                    if (!pointLayer->collection().isValid()) {
                        delete pointLayer;
                        continue;
                    }
                    const QJsonObject styleObject =
                        layerObject.value("point_style").toObject();
                    if (!styleObject.isEmpty()) {
                        pointLayer->setStyle(pointStyleFromJson(
                            styleObject, pointLayer->style()));
                    }
                    layer = pointLayer;
                } else {
                    auto *imageLayer = new ImageLayer(sourcePath);
                    if (imageLayer->rasterData().isNull()) {
                        delete imageLayer;
                        continue;
                    }
                    const QJsonObject styleObject =
                        layerObject.value("image_style").toObject();
                    if (!styleObject.isEmpty()) {
                        imageLayer->setStyle(imageStyleFromJson(
                            styleObject, imageLayer->style()));
                    }
                    layer = imageLayer;
                    ++loadedImageCount;
                }

                const QString savedName =
                    layerObject.value("name").toString();
                if (!savedName.isEmpty())
                    layer->rename(savedName);
                layer->setVisible(
                    layerObject.value("visible").toBool(layer->isVisible()));
                manager->addLayer(layer);
            }

            // Very early session files may not contain a layer inventory.
            // Keep their previous behavior without involving autoload for
            // sessions that do contain explicit layers.
            if (savedLayers.isEmpty()
                && loadImageIntoViewer(viewer, absolutePath, false, false)) {
                loadedImageCount = 1;
            }

            if (loadedImageCount == 0) {
                delete viewer;
                continue;
            }

            viewer->fitLoadedImage();
            viewer->setPrimarySourcePath(absolutePath);
            configureViewer(viewer);
            splitter->addWidget(viewer);

            const QJsonArray transformValues =
                panelObject.value("transform").toArray();
            const QJsonArray centerValues =
                panelObject.value("center").toArray();
            if (transformValues.size() == 9 && centerValues.size() == 2) {
                const QTransform transform(
                    transformValues[0].toDouble(),
                    transformValues[1].toDouble(),
                    transformValues[2].toDouble(),
                    transformValues[3].toDouble(),
                    transformValues[4].toDouble(),
                    transformValues[5].toDouble(),
                    transformValues[6].toDouble(),
                    transformValues[7].toDouble(),
                    transformValues[8].toDouble());
                const QPointF center(
                    centerValues[0].toDouble(),
                    centerValues[1].toDouble());
                QPointer<ImageViewer> guardedViewer = viewer;
                QTimer::singleShot(0, this, [guardedViewer, transform, center]() {
                    if (guardedViewer)
                        guardedViewer->restoreView(transform, center);
                });
            }

            if (panelIndex == activePanel)
                requestedActiveViewer = viewer;
            ++restoredPanels;
        }

        if (restoredPanels == 0) {
            delete splitter;
            continue;
        }

        const QJsonArray savedSizes =
            tabObject.value("split_sizes").toArray();
        QList<int> sizes;
        for (const QJsonValue &size : savedSizes)
            sizes.append(size.toInt());
        if (sizes.size() == splitter->count())
            splitter->setSizes(sizes);

        QString title = tabObject.value("title").toString();
        if (title.isEmpty()) {
            auto *viewer =
                qobject_cast<ImageViewer*>(splitter->widget(0));
            title = viewer && !viewer->primarySourcePath().isEmpty()
                ? QFileInfo(viewer->primarySourcePath()).fileName()
                : QStringLiteral("Viewer");
        }
        ui->workspaceTabs->addTab(splitter, title);

        auto *firstViewer =
            qobject_cast<ImageViewer*>(splitter->widget(0));
        if (firstViewer && !firstViewer->primarySourcePath().isEmpty()) {
            m_openImageTabs.insert(
                firstViewer->primarySourcePath(), splitter);
        }
    }

    if (ui->workspaceTabs->count() == 0) {
        updateViewerEmptyState();
        return;
    }

    restoredCurrentTab = qBound(
        0, restoredCurrentTab, ui->workspaceTabs->count() - 1);
    ui->workspaceTabs->setCurrentIndex(restoredCurrentTab);

    if (!requestedActiveViewer
        || ui->workspaceTabs->indexOf(
            requestedActiveViewer->parentWidget()) != restoredCurrentTab) {
        auto *splitter = qobject_cast<QSplitter*>(
            ui->workspaceTabs->widget(restoredCurrentTab));
        requestedActiveViewer = splitter
            ? qobject_cast<ImageViewer*>(splitter->widget(0))
            : nullptr;
    }

    m_activeViewer = requestedActiveViewer;
    onActiveViewerChanged(requestedActiveViewer);
    if (requestedActiveViewer)
        requestedActiveViewer->setFocus();

    const bool sync =
        session.value("sync_split_views").toBool(false);
    ui->actionSyncSplitViews->setChecked(sync);
    ImageViewer::setSyncEnabled(sync);
    const bool syncZoom =
        session.value("sync_split_zoom").toBool(true);
    ui->actionSyncSplitZoom->setChecked(syncZoom);
    ImageViewer::setSyncZoomEnabled(syncZoom);
    updateProjectActions();
    updateViewerEmptyState();
    refreshReloadableArtifactWatchers();
}

void MainWindow::updateProjectExplorer()
{
    updateProjectHeader();

    if (!m_project.isValid()) {
        m_projectFileMarks.clear();
        m_projectFileSystemModel->setFileMarks(m_projectFileMarks);
        resetProjectExplorerToFileSystemRoot();
        updateProjectExplorerStarredEmptyMessage();
        return;
    }
    m_projectFileSystemModel->setProjectRoot(m_project.rootPath());
    auto *proxy =
        static_cast<ProjectExplorerSortProxyModel*>(m_projectSortProxy);
    proxy->setProjectRootPath(m_project.rootPath());
    m_projectFileSystemModel->setRootPath(m_project.rootPath());
    refreshProjectExplorerRootIndex();
    ui->projectTree->setColumnWidth(0, 220);
    updateProjectExplorerStarredEmptyMessage();
}

void MainWindow::resetProjectExplorerToFileSystemRoot()
{
    if (!ui->projectTree || !m_projectFileSystemModel || !m_projectSortProxy)
        return;

    auto *proxy =
        static_cast<ProjectExplorerSortProxyModel*>(m_projectSortProxy);
    proxy->setProjectRootPath(QString());
    m_projectFileSystemModel->setProjectRoot(QString());

    const QModelIndex sourceRoot =
        m_projectFileSystemModel->setRootPath(QString());
    const QModelIndex proxyRoot = projectProxyIndex(sourceRoot);
    ui->projectTree->setRootIndex(
        proxyRoot.isValid() ? proxyRoot : QModelIndex());
}

void MainWindow::updateProjectHeader()
{
    if (!m_projectHeaderWidget
        || !m_projectDescriptionLabel
        || !m_projectPathLabel) {
        return;
    }

    const bool hasProject = m_project.isValid();
    m_projectHeaderWidget->setVisible(hasProject);
    if (!hasProject)
        return;

    QString description = m_project.description().trimmed();
    if (description.isEmpty())
        description = m_project.name();
    const QString path =
        QDir::toNativeSeparators(QDir::cleanPath(m_project.rootPath()));

    if (auto *label =
            dynamic_cast<ElidedLabel*>(m_projectDescriptionLabel)) {
        label->setFullText(description);
    } else {
        m_projectDescriptionLabel->setText(description);
    }

    if (auto *label = dynamic_cast<ElidedLabel*>(m_projectPathLabel)) {
        label->setFullText(path);
    } else {
        m_projectPathLabel->setText(path);
    }
}

void MainWindow::refreshProjectExplorerRootIndex()
{
    if (!m_project.isValid() || !m_projectFileSystemModel || !m_projectSortProxy)
        return;

    const QModelIndex sourceRoot =
        m_projectFileSystemModel->index(m_project.rootPath());
    const QModelIndex proxyRoot = projectProxyIndex(sourceRoot);
    ui->projectTree->setRootIndex(proxyRoot);
    expandProjectExplorerGroups();
}

void MainWindow::expandProjectExplorerGroups()
{
    if (!m_project.isValid() || !ui->projectTree || !m_projectSortProxy)
        return;

    const QModelIndex root = ui->projectTree->rootIndex();
    if (!root.isValid())
        return;

    const int rows = m_projectSortProxy->rowCount(root);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_projectSortProxy->index(row, 0, root);
        if (!index.data(ProjectExplorerGroupRole).toBool())
            continue;

        const QString groupId =
            index.data(ProjectExplorerGroupIdRole).toString();
        const bool shouldExpand =
            !m_projectExplorerGroupExpansion.contains(groupId)
            || m_projectExplorerGroupExpansion.value(groupId);
        ui->projectTree->setExpanded(index, shouldExpand);
    }
}

MainWindow::ProjectExplorerViewState MainWindow::projectExplorerViewState()
    const
{
    ProjectExplorerViewState state;
    if (!m_project.isValid()
        || !ui->projectTree
        || !m_projectFileSystemModel
        || !m_projectSortProxy) {
        return state;
    }

    const QModelIndex topIndex =
        ui->projectTree->indexAt(QPoint(0, 0)).siblingAtColumn(0);
    if (topIndex.isValid()) {
        if (topIndex.data(ProjectExplorerGroupRole).toBool()) {
            state.topGroupId =
                topIndex.data(ProjectExplorerGroupIdRole).toString();
        } else {
            state.topPath = projectPathFromIndex(topIndex);
        }
    }

    QVector<QModelIndex> stack;
    const QModelIndex root = ui->projectTree->rootIndex();
    if (root.isValid())
        stack.append(root);

    while (!stack.isEmpty()) {
        const QModelIndex parent = stack.takeLast();
        const int rows = m_projectSortProxy->rowCount(parent);
        for (int row = rows - 1; row >= 0; --row) {
            const QModelIndex index = m_projectSortProxy->index(row, 0, parent);
            if (!index.isValid())
                continue;

            if (index.data(ProjectExplorerGroupRole).toBool()) {
                if (ui->projectTree->isExpanded(index))
                    stack.append(index);
                continue;
            }

            const QModelIndex sourceIndex = projectSourceIndex(index);
            if (!sourceIndex.isValid())
                continue;

            const QFileInfo info =
                m_projectFileSystemModel->fileInfo(sourceIndex);
            if (!info.isDir())
                continue;

            if (ui->projectTree->isExpanded(index)) {
                state.expandedPaths.append(
                    QDir::cleanPath(info.absoluteFilePath()));
                stack.append(index);
            }
        }
    }

    state.expandedPaths.removeDuplicates();
    return state;
}

void MainWindow::restoreProjectExplorerViewState(
    const ProjectExplorerViewState &state)
{
    if (!m_project.isValid()
        || !ui->projectTree
        || !m_projectFileSystemModel
        || !m_projectSortProxy) {
        return;
    }

    expandProjectExplorerGroups();

    for (const QString &path : state.expandedPaths) {
        const QFileInfo info(path);
        if (!info.isDir())
            continue;

        const QModelIndex sourceIndex =
            m_projectFileSystemModel->index(info.absoluteFilePath());
        const QModelIndex proxyIndex = projectProxyIndex(sourceIndex);
        if (proxyIndex.isValid())
            ui->projectTree->setExpanded(proxyIndex, true);
    }

    QModelIndex anchor;
    if (!state.topPath.isEmpty()) {
        const QModelIndex sourceIndex =
            m_projectFileSystemModel->index(state.topPath);
        anchor = projectProxyIndex(sourceIndex);
    } else if (!state.topGroupId.isEmpty()) {
        const QModelIndex root = ui->projectTree->rootIndex();
        const int rows = m_projectSortProxy->rowCount(root);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex index =
                m_projectSortProxy->index(row, 0, root);
            if (index.data(ProjectExplorerGroupIdRole).toString()
                == state.topGroupId) {
                anchor = index;
                break;
            }
        }
    }

    if (anchor.isValid())
        ui->projectTree->scrollTo(anchor, QAbstractItemView::PositionAtTop);
}

void MainWindow::rememberProjectExplorerGroupExpansion(
    const QModelIndex &index,
    bool expanded)
{
    if (!index.data(ProjectExplorerGroupRole).toBool())
        return;

    const QString groupId =
        index.data(ProjectExplorerGroupIdRole).toString();
    if (!groupId.isEmpty())
        m_projectExplorerGroupExpansion.insert(groupId, expanded);
}

void MainWindow::updateProjectExplorerStarredEmptyMessage()
{
    if (!m_projectStarredEmptyLabel || !m_starredOnlyCheck)
        return;

    const bool show =
        m_project.isValid()
        && m_starredOnlyCheck->isChecked()
        && m_projectFileMarks.isEmpty();
    m_projectStarredEmptyLabel->setVisible(show);
}

void MainWindow::updateMaskBrushControls(ImageViewer *viewer)
{
    const bool editing = viewer
        && viewer->maskEditEnabled()
        && viewer->editingMaskLayer()
        && viewer->editingMaskLayer()->binaryMaskEditMode();
    const int maximum = viewer ? viewer->maskBrushMaximum() : 200;
    const int value = viewer ? viewer->maskBrushSize() : 9;

    if (m_maskBrushSpin) {
        const QSignalBlocker blocker(m_maskBrushSpin);
        m_maskBrushSpin->setRange(1, maximum);
        m_maskBrushSpin->setValue(qBound(1, value, maximum));
        m_maskBrushSpin->setEnabled(editing);
    }

    if (m_maskBrushSlider) {
        const QSignalBlocker blocker(m_maskBrushSlider);
        m_maskBrushSlider->setValue(
            maskBrushSliderFromSize(qBound(1, value, maximum), maximum));
        m_maskBrushSlider->setEnabled(editing);
    }
}

void MainWindow::updateProjectActions()
{
    const bool hasProject = m_project.isValid();
    ui->actionSaveProjectAs->setEnabled(hasProject);
    ui->actionSaveProjectAsCleanCopy->setEnabled(hasProject);
    ui->actionEditProjectDescription->setEnabled(hasProject);
    ui->actionProjectReport->setEnabled(hasProject);
    ui->actionCloseProject->setEnabled(hasProject);
    ImageViewer *viewer = currentImageViewer();
    ui->actionPan->setEnabled(viewer != nullptr);
    ui->actionZoom->setEnabled(viewer != nullptr);
    auto *splitter = viewer
        ? qobject_cast<QSplitter*>(viewer->parentWidget())
        : nullptr;
    const bool hasSplitSiblings = splitter && splitter->count() > 1;
    ui->actionSyncSplitViews->setEnabled(hasSplitSiblings);
    ui->actionSyncSplitZoom->setEnabled(hasSplitSiblings);
    ui->actionImportRaw->setEnabled(hasProject);
    ui->actionImportLegacyJpegs->setEnabled(hasProject);
    ui->actionRegisterCollection->setEnabled(hasProject);
    ui->actionCheckCollections->setEnabled(hasProject);
    ui->actionCollectionGraph->setEnabled(hasProject);
    ui->actionCleanProject->setEnabled(hasProject);
    ui->actionCreatePoints->setEnabled(hasProject);
    ui->actionCreatePointGrid->setEnabled(hasProject);
    ui->actionSelectPointsByMask->setEnabled(hasProject);
    ui->actionCreateMask->setEnabled(hasProject);
    ui->actionBuildMetadataRules->setEnabled(hasProject);
    ui->actionValidateImageMetadata->setEnabled(hasProject);
    ui->actionValidateCollectionMetadata->setEnabled(hasProject);
    updateMaskEditControls(viewer);
    if (m_newFolderButton)
        m_newFolderButton->setEnabled(hasProject);
    if (m_unstarAllButton)
        m_unstarAllButton->setEnabled(hasProject && !m_projectFileMarks.isEmpty());
    if (m_collectionGraphButton)
        m_collectionGraphButton->setEnabled(hasProject);
    if (m_checkCollectionsButton)
        m_checkCollectionsButton->setEnabled(hasProject);
    if (m_starredOnlyCheck)
        m_starredOnlyCheck->setEnabled(hasProject);

    bool hasRawFiles = false;
    if (hasProject) {
        const QString rawDir = QDir(m_project.rootPath()).filePath("raw_files");
        const QStringList rawExtensions = {
            "cr2", "cr3", "nef", "arw", "dng", "raf", "orf",
            "rw2", "pef", "srw", "raw", "dcr", "kdc"
        };
        QDirIterator it(rawDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (rawExtensions.contains(
                    QFileInfo(it.next()).suffix().toLower())) {
                hasRawFiles = true;
                break;
            }
        }
    }
    ui->actionExtractRawData->setEnabled(hasRawFiles);
}

void MainWindow::buildMetadataRules()
{
    if (!m_project.isValid())
        return;

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }

    const QString startDirectory =
        QDir(m_project.rootPath()).filePath("raw_files");
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Select Calibration Images",
        QFileInfo::exists(startDirectory)
            ? startDirectory : m_project.rootPath(),
        "Images (*.nef *.NEF *.cr2 *.CR2 *.cr3 *.CR3 *.arw *.ARW "
        "*.dng *.DNG *.raf *.RAF *.orf *.ORF *.rw2 *.RW2 "
        "*.tif *.tiff *.TIF *.TIFF *.jpg *.jpeg *.JPG *.JPEG);;"
        "All files (*)");
    if (files.isEmpty())
        return;
    for (const QString &file : files) {
        if (!pathBelongsToProject(m_project.rootPath(), file)) {
            QMessageBox::warning(
                this, "Portable Metadata Draft",
                "Calibration images must be inside the current project so "
                "the draft remains portable.");
            return;
        }
    }

    if (files.size() == 1
        && QMessageBox::question(
               this, "Single Calibration Image",
               "Using several calibration images makes constant metadata "
               "easier to recognize. Continue with one image?",
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    beginStatusProgress(
        QStringLiteral("Extracting metadata from %1 calibration image%2...")
            .arg(files.size())
            .arg(files.size() == 1 ? "" : "s"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<MetadataObservation> observations;
    const bool extracted =
        ExifToolService::extract(files, &observations, &errorMessage);
    const QString version = extracted
        ? ExifToolService::version(&errorMessage) : QString();
    QApplication::restoreOverrideCursor();
    endStatusProgress(
        extracted && !version.isEmpty()
            ? QStringLiteral("Metadata extracted")
            : QStringLiteral("Metadata extraction failed"),
        3000);
    if (!extracted || version.isEmpty()) {
        QMessageBox::critical(this, "Metadata Extraction", errorMessage);
        return;
    }

    MetadataRulesDialog dialog(
        MetadataRegistry::analyze(files, observations), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString draftDir =
        QDir(m_project.rootPath()).filePath("registry_drafts");
    if (!QDir().mkpath(draftDir)) {
        QMessageBox::critical(
            this, "Save Metadata Rules",
            "Could not create the registry_drafts folder.");
        return;
    }

    QString defaultDraftName =
        MetadataRegistry::snakeCase(QFileInfo(m_project.rootPath()).fileName());
    if (defaultDraftName.isEmpty())
        defaultDraftName = "metadata_rules_draft";

    QFileDialog saveDialog(this, "Save Metadata Rules Draft");
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setDirectory(draftDir);
    saveDialog.setNameFilter("YAML files (*.yaml *.yml)");
    saveDialog.setDefaultSuffix("yaml");
    saveDialog.setOption(QFileDialog::DontConfirmOverwrite, true);
    saveDialog.selectFile(defaultDraftName + ".yaml");
    if (saveDialog.exec() != QDialog::Accepted)
        return;

    QString draftPath = saveDialog.selectedFiles().value(0);
    QFileInfo draftInfo(draftPath);
    if (draftInfo.suffix().isEmpty()) {
        draftPath += ".yaml";
        draftInfo.setFile(draftPath);
    }
    const QString suffix = draftInfo.suffix().toLower();
    if (suffix != "yaml" && suffix != "yml") {
        QMessageBox::warning(
            this, "Save Metadata Rules",
            "Metadata rules drafts must be saved as .yaml or .yml files.");
        return;
    }
    if (!pathBelongsToProject(m_project.rootPath(), draftPath)) {
        QMessageBox::warning(
            this, "Portable Metadata Draft",
            "Save the metadata rules draft inside the current project.");
        return;
    }

    const QString snippetPath =
        QFileInfo(draftPath).absolutePath() + "/"
        + QFileInfo(draftPath).completeBaseName() + "_snippet.R";
    if (QFileInfo::exists(draftPath) || QFileInfo::exists(snippetPath)) {
        QStringList artifacts;
        if (QFileInfo::exists(draftPath))
            artifacts.append(QFileInfo(draftPath).fileName());
        if (QFileInfo::exists(snippetPath))
            artifacts.append(QFileInfo(snippetPath).fileName());
        if (QMessageBox::question(
                this, "Replace Metadata Rules Draft",
                QString("This will overwrite:\n- %1\n\nContinue?")
                    .arg(artifacts.join("\n- ")),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
    }
    if (!MetadataRegistry::writeDraft(
            draftPath, m_project.rootPath(), files,
            dialog.selectedRules(), version, &errorMessage)) {
        QMessageBox::critical(this, "Save Metadata Rules", errorMessage);
        return;
    }
    updateProjectExplorer();
    QMessageBox::information(
        this, "Metadata Rules",
        QString("Draft and R snippet saved to:\n%1\n%2")
            .arg(QDir::toNativeSeparators(draftPath),
                 QDir::toNativeSeparators(snippetPath)));
}

void MainWindow::validateImageMetadata()
{
    if (!m_project.isValid())
        return;

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }

    QString initialPath = selectedProjectPath();
    if (!QFileInfo(initialPath).exists())
        initialPath = m_project.rootPath();
    const QString imagePath = QFileDialog::getOpenFileName(
        this, "Select Image", initialPath,
        "Images (*.nef *.NEF *.cr2 *.CR2 *.cr3 *.CR3 *.arw *.ARW "
        "*.dng *.DNG *.raf *.RAF *.orf *.ORF *.rw2 *.RW2 "
        "*.tif *.tiff *.TIF *.TIFF *.jpg *.jpeg *.JPG *.JPEG);;"
        "All files (*)");
    if (imagePath.isEmpty())
        return;
    if (!pathBelongsToProject(m_project.rootPath(), imagePath)) {
        QMessageBox::warning(
            this, "Portable Registry Review",
            "The image must be inside the current project.");
        return;
    }

    const Collection collection = collectionForPath(imagePath);
    if (CollectionValidation::isEligible(
            m_project.rootPath(), collection)) {
        const QList<CollectionFileValidation> pairs =
            CollectionValidation::resolveRawPairs(
                m_project.rootPath(), collection);
        for (const CollectionFileValidation &pair : pairs) {
            if (!samePath(pair.preprocessedPath, imagePath)) {
                continue;
            }
            if (pair.status
                == CollectionFileValidationStatus::MissingRaw) {
                QMessageBox::warning(
                    this, "Validate Image",
                    QString(
                        "No RAW with basename \"%1\" was found in raw_files.")
                        .arg(pair.baseName));
                return;
            }
            if (pair.status
                == CollectionFileValidationStatus::AmbiguousRaw) {
                QMessageBox::warning(
                    this, "Validate Image",
                    QString(
                        "More than one RAW with basename \"%1\" was found:\n%2")
                        .arg(pair.baseName,
                             pair.rawCandidates.join("\n")));
                return;
            }
            validateRawImage(
                pair.rawPath,
                [&]() {
                    QString registryPath;
                    CollectionValidation::resolveRegistry(
                        m_project.rootPath(),
                        collection, &registryPath);
                    return registryPath;
                }());
            return;
        }
    }
    validateRawImage(imagePath);
}

bool MainWindow::validateRawImage(
    const QString &rawPath,
    const QString &providedRegistryPath)
{
    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return false;
    }
    QString registryPath = providedRegistryPath;
    if (registryPath.isEmpty()) {
        const QString rulesStart =
            QDir(m_project.rootPath()).filePath("registry_drafts");
        registryPath = QFileDialog::getOpenFileName(
            this, "Select Registry Snapshot or Rules Draft", rulesStart,
            "YAML files (*.yaml *.yml)");
    }
    if (registryPath.isEmpty())
        return false;
    if (!pathBelongsToProject(m_project.rootPath(), registryPath)) {
        QMessageBox::warning(
            this, "Portable Registry Review",
            "The snapshot or draft must be inside the current project.");
        return false;
    }

    QList<MetadataRule> rules;
    if (!MetadataRegistry::readRules(
            registryPath, &rules, &errorMessage)) {
        QMessageBox::critical(this, "Registry Rules", errorMessage);
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<MetadataObservation> observations;
    const bool extracted = ExifToolService::extract(
        {rawPath}, &observations, &errorMessage);
    const QString version = extracted
        ? ExifToolService::version(&errorMessage) : QString();
    QApplication::restoreOverrideCursor();
    if (!extracted || version.isEmpty()) {
        QMessageBox::critical(this, "Metadata Extraction", errorMessage);
        return false;
    }

    RegistryValidationDialog dialog(rules, observations, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString stem =
        MetadataRegistry::snakeCase(QFileInfo(rawPath).completeBaseName());
    const QString timestamp =
        QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
    const QString reviewPath =
        QDir(m_project.rootPath()).filePath(
            QString("registry_reviews/%1_%2.yaml").arg(stem, timestamp));
    if (!MetadataRegistry::writeReview(
            reviewPath, m_project.rootPath(), rawPath, registryPath,
            version, dialog.decisions(), dialog.acceptedDecision(),
            &errorMessage)) {
        QMessageBox::critical(this, "Save Registry Review", errorMessage);
        return false;
    }
    updateProjectExplorer();
    QMessageBox::information(
        this, "Registry Review",
        QString("%1 review saved to:\n%2")
            .arg(dialog.acceptedDecision() ? "Accepted" : "Rejected",
                 QDir::toNativeSeparators(reviewPath)));
    return true;
}

void MainWindow::validateCollectionMetadata()
{
    if (!m_project.isValid())
        return;

    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    QVector<Collection> eligible;
    for (const Collection &collection : collections) {
        if (CollectionValidation::isEligible(
                m_project.rootPath(), collection)) {
            eligible.append(collection);
        }
    }
    if (eligible.isEmpty()) {
        QStringList diagnostics;
        for (const Collection &collection : collections) {
            if (collection.collectionClass
                != "preprocessed_image_collection") {
                continue;
            }
            QString registryPath;
            QString reason;
            CollectionValidation::resolveRegistry(
                m_project.rootPath(), collection,
                &registryPath, &reason);
            diagnostics.append(
                QString("%1: %2").arg(collection.dirName, reason));
        }
        QMessageBox::information(
            this, "Validate Collection",
            diagnostics.isEmpty()
                ? QString(
                    "No preprocessed image collection was found.")
                : QString(
                    "No preprocessed image collection has a usable registry.\n\n%1")
                    .arg(diagnostics.join("\n")));
        return;
    }

    Collection selected = collectionForPath(selectedProjectPath());
    if (!CollectionValidation::isEligible(
            m_project.rootPath(), selected)) {
        QStringList names;
        for (const Collection &collection : eligible)
            names.append(collection.dirName);
        bool accepted = false;
        const QString name = QInputDialog::getItem(
            this, "Validate Collection", "Collection:",
            names, 0, false, &accepted);
        if (!accepted)
            return;
        for (const Collection &collection : eligible) {
            if (collection.dirName == name) {
                selected = collection;
                break;
            }
        }
    }

    QString errorMessage;
    if (!ExifToolService::isAvailable(&errorMessage)) {
        QMessageBox::critical(this, "ExifTool", errorMessage);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    CollectionValidationRun run;
    const bool validated = CollectionValidation::validate(
        m_project.rootPath(), selected, &run, &errorMessage);
    QApplication::restoreOverrideCursor();
    if (!validated) {
        QMessageBox::critical(
            this, "Validate Collection", errorMessage);
        return;
    }

    const QString manifestPath =
        QDir(m_project.rootPath()).filePath(
            QString("registry_validations/%1_%2.yaml")
                .arg(MetadataRegistry::snakeCase(selected.dirName),
                     QDateTime::currentDateTimeUtc().toString(
                         "yyyyMMdd_HHmmss")));
    if (!CollectionValidation::writeManifest(
            manifestPath, m_project.rootPath(), run, &errorMessage)) {
        QMessageBox::critical(
            this, "Validate Collection", errorMessage);
        return;
    }
    m_projectFileSystemModel->refreshDirectory(
        QDir(m_project.rootPath()).filePath("registry_validations"));

    CollectionValidationDialog dialog(run, this);
    connect(&dialog, &CollectionValidationDialog::reviewRequested,
            this, [this](const QString &rawPath,
                         const QString &registryPath) {
                validateRawImage(rawPath, registryPath);
            });
    dialog.exec();
}

Collection MainWindow::collectionForPath(const QString &path) const
{
    if (!m_project.isValid() || path.isEmpty())
        return {};
    const QString absolute = QFileInfo(path).isDir()
        ? QFileInfo(path).absoluteFilePath()
        : QFileInfo(path).absolutePath();
    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    const int collectionIndex = collectionIndexByDirectory(
        m_project.rootPath(), collections, absolute);
    return collectionIndex >= 0 ? collections.at(collectionIndex) : Collection();
}

bool MainWindow::isEditableMaskLayer(ImageLayer *layer) const
{
    if (!layer || !layer->isEditableBinaryMask())
        return false;

    const Collection collection = collectionForPath(layer->sourcePath());
    return collection.collectionClass == "segmentation_collection";
}

void MainWindow::updateMaskEditControls(ImageViewer *viewer)
{
    const bool hasViewer = viewer != nullptr;
    ImageLayer *activeImageLayer = nullptr;
    if (hasViewer && viewer->layerManager())
        activeImageLayer =
            qobject_cast<ImageLayer*>(viewer->layerManager()->activeLayer());

    ImageLayer *editingLayer =
        hasViewer ? viewer->editingMaskLayer() : nullptr;
    const bool editing =
        hasViewer
        && viewer->maskEditEnabled()
        && editingLayer
        && editingLayer->binaryMaskEditMode();
    const bool canEdit = editing || isEditableMaskLayer(activeImageLayer);

    {
        const QSignalBlocker blocker(ui->actionEditMask);
        ui->actionEditMask->setChecked(editing);
        ui->actionEditMask->setEnabled(canEdit);
    }
    ui->actionCancelMaskEdit->setEnabled(editing);
    ui->actionInvertMask->setEnabled(editing);

    updateMaskBrushControls(viewer);
}

ImageLayer *MainWindow::createStyledImageLayer(const QString &path)
{
    const QFileInfo info(path);
    beginStatusProgress(
        QStringLiteral("Loading TIFF: %1").arg(info.fileName()));

    auto *layer = new ImageLayer(path);
    if (layer->rasterData().isNull()) {
        delete layer;
        endStatusProgress(QStringLiteral("Could not load TIFF"), 3000);
        QMessageBox::warning(this, "Add TIFF", "Could not load TIFF.");
        return nullptr;
    }

    const Collection collection = collectionForPath(path);
    if (!collection.collectionClass.isEmpty()) {
        CollectionStyleResolver styleResolver;
        layer->setStyle(styleResolver.imageStyle(
            collection, layer->rasterData()));
    }
    endStatusProgress(
        QStringLiteral("Loaded TIFF: %1").arg(info.fileName()),
        3000);
    return layer;
}

PointLayer *MainWindow::createStyledPointLayer(const QString &path,
                                               ImageViewer *viewer)
{
    if (!viewer)
        return nullptr;

    QString errorMessage;
    const PointCollection pointCollection =
        PointCollection::load(path, &errorMessage);
    if (!pointCollection.isValid()) {
        QMessageBox::warning(this, "Add CSV",
                             pointCsvContractMessage(errorMessage));
        return nullptr;
    }

    auto *layer = new PointLayer(path, viewer->scene());
    const Collection collection = collectionForPath(path);
    if (collection.collectionClass == "point_collection"
        && layer->collection().isValid()) {
        CollectionStyleResolver styleResolver;
        layer->setStyle(styleResolver.pointStyle(
            collection, layer->collection().recordCount()));
    }
    return layer;
}

ImageViewer *MainWindow::currentImageViewer() const
{
    if (m_activeViewer)
        return m_activeViewer;

    const int index = ui->workspaceTabs->currentIndex();
    if (index < 0)
        return nullptr;

    auto *splitter = qobject_cast<QSplitter*>(ui->workspaceTabs->widget(index));
    if (!splitter || splitter->count() == 0)
        return nullptr;

    return qobject_cast<ImageViewer*>(splitter->widget(0));
}

void MainWindow::updatePropertiesTable(const PixelInfo &info)
{
    if (!ui->propertiesTable)
        return;

    ui->propertiesTable->setRowCount(0);
    if (info.row < 0 || info.bandCount <= 0)
        return;

    ui->propertiesTable->setRowCount(2 + info.bandCount);

    ui->propertiesTable->setItem(0, 0, new QTableWidgetItem("Row"));
    ui->propertiesTable->setItem(0, 1, new QTableWidgetItem(QString::number(info.row)));
    ui->propertiesTable->setItem(1, 0, new QTableWidgetItem("Column"));
    ui->propertiesTable->setItem(1, 1, new QTableWidgetItem(QString::number(info.col)));

    for (int b = 0; b < info.bandCount; ++b) {
        ui->propertiesTable->setItem(2 + b, 0,
            new QTableWidgetItem(QStringLiteral("Band %1").arg(b + 1)));
        ui->propertiesTable->setItem(2 + b, 1,
            new QTableWidgetItem(info.bandValues.value(b).toString()));
    }
}

void MainWindow::updatePropertiesForPointLayer(PointLayer *layer)
{
    Q_UNUSED(layer);
    onPointSelectionChanged();
}

void MainWindow::onSyncSplitViewsToggled(bool checked)
{
    ImageViewer::setSyncEnabled(checked);
}

void MainWindow::onSyncSplitZoomToggled(bool checked)
{
    ImageViewer::setSyncZoomEnabled(checked);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const DroppedArtifactBatch batch =
        classifyDroppedArtifacts(event->mimeData()->urls());
    const bool hasSupported =
        !batch.tiffPaths.isEmpty() || !batch.csvPaths.isEmpty();
    if (hasSupported && batch.unsupportedPaths.isEmpty())
        event->acceptProposedAction();
    else
        event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const DroppedArtifactBatch batch =
        classifyDroppedArtifacts(event->mimeData()->urls());
    if (!batch.unsupportedPaths.isEmpty()) {
        QMessageBox::information(
            this, "Drop files",
            "Only TIFF image files and compatible point CSV files can be dropped here.");
        event->ignore();
        return;
    }

    if (!batch.tiffPaths.isEmpty() && !batch.csvPaths.isEmpty()) {
        QMessageBox::information(
            this, "Drop files",
            "Drop TIFF files here to open viewers. Drop CSV files directly "
            "onto an image panel or the Layers dock.");
        event->ignore();
        return;
    }

    if (!batch.csvPaths.isEmpty()) {
        QMessageBox::information(
            this, "Add CSV",
            "Drop point CSV files directly onto an image panel or the Layers dock.");
        event->acceptProposedAction();
        return;
    }

    if (batch.tiffPaths.isEmpty()) {
        event->ignore();
        return;
    }

    for (const QString &path : batch.tiffPaths)
        openImageAtPath(path);
    event->acceptProposedAction();
}

void MainWindow::clearPropertiesTable()
{
    if (ui->propertiesTable)
        ui->propertiesTable->setRowCount(0);
}

void MainWindow::configureDockSizing()
{
    if (ui->centralwidget) {
        ui->centralwidget->setMinimumSize(0, 0);
        ui->centralwidget->setMaximumSize(0, 0);
        ui->centralwidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    auto constrainWidth = [](QDockWidget *dock, int minimumWidth, int maximumWidth) {
        if (!dock)
            return;

        dock->setMinimumWidth(minimumWidth);
        dock->setMaximumWidth(maximumWidth);

        if (QWidget *contents = dock->widget()) {
            contents->setMinimumWidth(minimumWidth);
            contents->setMaximumWidth(maximumWidth);
        }
    };

    constrainWidth(ui->dockProjectExplorer, 260, 460);
    constrainWidth(m_previewDock, 260, 460);
    constrainWidth(ui->dockLayers, 220, 320);
    constrainWidth(ui->dockProperties, 220, 320);
    constrainWidth(m_histogramDock, 220, 320);

    if (ui->dockViewer) {
        ui->dockViewer->setMinimumWidth(320);
        if (QWidget *contents = ui->dockViewer->widget())
            contents->setMinimumWidth(320);
    }
}

QString MainWindow::selectedProjectPath() const
{
    const QModelIndex index = ui->projectTree->currentIndex();
    if (!index.isValid())
        return {};

    const QFileInfo fi(projectPathFromIndex(index));
    if (!fi.isDir())
        return {};

    return fi.absoluteFilePath();
}

void MainWindow::createPointsCsv()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Create points",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QVector<Collection> collections = Project::discoverCollections(m_project.rootPath());
    QVector<Collection> pointCollections;
    for (const Collection &col : collections) {
        if (col.collectionClass == "point_collection")
            pointCollections.append(col);
    }

    if (pointCollections.isEmpty()) {
        QMessageBox::information(this, "Create points",
                                 "No point collection is registered in this project.\n\n"
                                 "A point collection is a folder registered as a point collection.\n\n"
                                 "To register a folder as a point collection, use Project > Register collection...\n\n"
                                 "If you need an empty folder, create one in Project Explorer with the New Folder button.");
        return;
    }

    Collection selectedCollection = collectionForPath(selectedProjectPath());
    if (selectedCollection.collectionClass != "point_collection") {
        selectedCollection = {};
    }

    if (selectedCollection.collectionClass != "point_collection"
        && pointCollections.size() == 1) {
        selectedCollection = pointCollections.first();
    } else if (selectedCollection.collectionClass != "point_collection") {
        QStringList collectionNames;
        for (const Collection &collection : pointCollections)
            collectionNames.append(collection.dirName);
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select point collection",
                                                     "Point collection:", collectionNames, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        for (const Collection &collection : pointCollections) {
            if (collection.dirName == choice) {
                selectedCollection = collection;
                break;
            }
        }
    }

    const QString selectedDir = selectedCollection.dirPath;
    QSet<QString> usedBaseNames;
    const QFileInfoList existingPointFiles = QDir(selectedDir).entryInfoList(
        {"*.csv", "*.CSV"}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &file : existingPointFiles)
        usedBaseNames.insert(file.completeBaseName().toCaseFolded());

    QStringList availableBaseNames;
    const QString parentPath = Project::resolveReference(
        m_project.rootPath(), selectedCollection.parent);
    if (QDir(parentPath).exists()) {
        const QFileInfoList parentImages = QDir(parentPath).entryInfoList(
            {"*.tif", "*.tiff", "*.TIF", "*.TIFF",
             "*.png", "*.PNG", "*.jpg", "*.jpeg", "*.JPG", "*.JPEG",
             "*.bmp", "*.BMP"},
            QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &image : parentImages) {
            const QString baseName = image.completeBaseName();
            if (!usedBaseNames.contains(baseName.toCaseFolded())
                && !availableBaseNames.contains(
                    baseName, Qt::CaseInsensitive)) {
                availableBaseNames.append(baseName);
            }
        }
    }

    const QString collectionBaseName =
        usedBaseNames.contains(selectedCollection.dirName.toCaseFolded())
        ? QString()
        : selectedCollection.dirName;
    PointCsvCreationDialog dialog(
        collectionBaseName, availableBaseNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedBaseNames = dialog.selectedBaseNames();
    if (selectedBaseNames.isEmpty())
        return;

    QStringList savePaths;
    savePaths.reserve(selectedBaseNames.size());
    for (const QString &baseName : selectedBaseNames) {
        const QString savePath = QDir(selectedDir).filePath(baseName + ".csv");
        if (QFileInfo::exists(savePath)) {
            QMessageBox::warning(
                this,
                "Create points",
                QStringLiteral("Points file already exists:\n%1")
                    .arg(QDir::toNativeSeparators(savePath)));
            return;
        }
        savePaths.append(savePath);
    }

    const PointCollection collection = PointCollection::createEmpty({"row", "col"});
    QString errorMsg;
    for (const QString &savePath : std::as_const(savePaths)) {
        if (!collection.save(savePath, &errorMsg)) {
            QMessageBox::warning(this, "Create points", errorMsg);
            return;
        }
    }

    statusBar()->showMessage(
        savePaths.size() == 1
            ? QStringLiteral("Created points file: %1")
                  .arg(QFileInfo(savePaths.first()).fileName())
            : QStringLiteral("Created %1 point CSV files")
                  .arg(savePaths.size()),
        5000);
    m_projectFileSystemModel->refreshDirectory(selectedDir);

    // If an image viewer is active, offer to add the new layer
    ImageViewer *viewer = currentImageViewer();
    if (savePaths.size() == 1 && viewer && viewer->layerManager()) {
        const auto reply = QMessageBox::question(this, "Add layer",
                                                 "Add the new points file as a layer to the current viewer?");
        if (reply == QMessageBox::Yes) {
            if (PointLayer *layer = createStyledPointLayer(savePaths.first(), viewer))
                viewer->layerManager()->addLayer(layer);
        }
    }
}

void MainWindow::createPointGrid()
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Create grid",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    QVector<Collection> pointCollections;
    for (const Collection &collection : collections) {
        if (collection.collectionClass == "point_collection"
            && !normalizedProjectReference(
                    m_project.rootPath(), collection.parent).isEmpty()) {
            pointCollections.append(collection);
        }
    }

    if (pointCollections.isEmpty()) {
        QMessageBox::information(
            this, "Create grid",
            "No point collection with a valid parent is registered in this project.\n\n"
            "A point collection is a folder registered as a point collection.\n\n"
            "To register a folder as a point collection, use Project > Register collection...");
        return;
    }

    struct GridCandidate {
        int id = -1;
        Collection pointCollection;
        MaskCandidate mask;
    };

    QVector<GridCandidate> gridCandidates;
    for (const Collection &pointCollection : std::as_const(pointCollections)) {
        const QVector<MaskCandidate> masks = compatibleMaskCandidates(
            m_project.rootPath(), pointCollection, collections);
        for (const MaskCandidate &mask : masks)
            gridCandidates.append(GridCandidate{
                static_cast<int>(gridCandidates.size()),
                pointCollection,
                mask
            });
    }

    if (gridCandidates.isEmpty()) {
        QMessageBox::information(
            this, "Create grid",
            "No compatible mask was found.\n\n"
            "A compatible mask must be a TIFF in a segmentation collection linked "
            "to the same parent image collection as the point collection.");
        return;
    }

    const auto sameCollection = [](const Collection &left,
                                   const Collection &right) {
        if (left.dirPath.isEmpty() || right.dirPath.isEmpty())
            return false;
#ifdef Q_OS_WIN
        const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
        return QDir::cleanPath(left.dirPath).compare(
                   QDir::cleanPath(right.dirPath), sensitivity) == 0;
    };

    const auto sameFile = [](const QString &left,
                             const QString &right) {
        if (left.isEmpty() || right.isEmpty())
            return false;
#ifdef Q_OS_WIN
        const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
        return QDir::cleanPath(QFileInfo(left).absoluteFilePath()).compare(
                   QDir::cleanPath(QFileInfo(right).absoluteFilePath()),
                   sensitivity) == 0;
    };

    const Collection selectedCollection = collectionForPath(selectedProjectPath());
    const QString selectedPath = selectedProjectPath();

    int preferredCandidateId = -1;
    if (selectedCollection.collectionClass == "segmentation_collection") {
        const QFileInfo selectedInfo(selectedPath);
        for (const GridCandidate &candidate : std::as_const(gridCandidates)) {
            if (selectedInfo.isFile()
                && sameFile(candidate.mask.path, selectedPath)) {
                preferredCandidateId = candidate.id;
                break;
            }
            if (preferredCandidateId < 0
                && sameCollection(candidate.mask.collection,
                                  selectedCollection)) {
                preferredCandidateId = candidate.id;
            }
        }
    }
    if (preferredCandidateId < 0
        && selectedCollection.collectionClass == "point_collection") {
        for (const GridCandidate &candidate : std::as_const(gridCandidates)) {
            if (sameCollection(candidate.pointCollection,
                               selectedCollection)) {
                preferredCandidateId = candidate.id;
                break;
            }
        }
    }
    if (preferredCandidateId < 0 && !gridCandidates.isEmpty())
        preferredCandidateId = gridCandidates.first().id;

    QVector<PointGridCreationDialog::Candidate> dialogCandidates;
    dialogCandidates.reserve(gridCandidates.size());
    for (const GridCandidate &candidate : std::as_const(gridCandidates)) {
        dialogCandidates.append(PointGridCreationDialog::Candidate{
            candidate.id,
            candidate.mask.label,
            candidate.mask.path,
            candidate.pointCollection.dirName,
            candidate.pointCollection.dirPath
        });
    }

    PointGridCreationDialog dialog(
        dialogCandidates,
        preferredCandidateId,
        this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QVector<PointGridCreationDialog::Task> tasks = dialog.tasks();
    if (tasks.isEmpty())
        return;

    const auto candidateById = [&gridCandidates](int id) -> const GridCandidate* {
        for (const GridCandidate &candidate : gridCandidates) {
            if (candidate.id == id)
                return &candidate;
        }
        return nullptr;
    };

    struct GridOutput {
        GridCandidate candidate;
        int stride = 1;
        QString savePath;
        QVector<QPoint> points;
    };

    QVector<GridOutput> outputs;
    outputs.reserve(tasks.size());
    QSet<QString> outputPathKeys;
    QStringList existingFiles;
    for (const PointGridCreationDialog::Task &task : tasks) {
        const GridCandidate *candidate = candidateById(task.candidateId);
        if (!candidate || task.stride < 1)
            return;

        const QString savePath = QDir(candidate->pointCollection.dirPath)
            .filePath(QFileInfo(candidate->mask.path).completeBaseName()
                      + ".csv");
        const QString savePathKey =
            normalizedAbsolutePathForComparison(savePath);
        if (outputPathKeys.contains(savePathKey)) {
            QMessageBox::warning(
                this,
                "Create grid",
                QStringLiteral(
                    "Two selected rows would write the same output file:\n%1")
                    .arg(QDir::toNativeSeparators(savePath)));
            return;
        }
        outputPathKeys.insert(savePathKey);

        if (QFileInfo::exists(savePath)) {
            existingFiles.append(
                QStringLiteral("- %1/%2")
                    .arg(candidate->pointCollection.dirName,
                         QFileInfo(savePath).fileName()));
        }

        outputs.append(GridOutput{*candidate, task.stride, savePath, {}});
    }

    if (!existingFiles.isEmpty()) {
        QStringList visibleFiles = existingFiles;
        if (visibleFiles.size() > 10) {
            visibleFiles = visibleFiles.mid(0, 10);
            visibleFiles.append(QStringLiteral("- ..."));
        }
        const auto answer = QMessageBox::question(
            this,
            "Create grid",
            QStringLiteral("Replace existing grid CSV file(s)?\n\n%1")
                .arg(visibleFiles.join('\n')),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    qint64 totalPoints = 0;
    for (GridOutput &output : outputs) {
        QString readError;
        const RasterData mask =
            TiffReader::load(output.candidate.mask.path, &readError);
        if (mask.isNull()) {
            QMessageBox::warning(
                this,
                "Create grid",
                QStringLiteral("Could not read mask TIFF:\n%1")
                    .arg(readError));
            return;
        }

        QString maskError;
        if (!validateBinaryMask(mask, &maskError)) {
            QMessageBox::warning(this, "Create grid", maskError);
            return;
        }

        const quint8 *maskValues = mask.bandPointer<quint8>(0);
        for (int row = 0; row < mask.height(); row += output.stride) {
            for (int col = 0; col < mask.width(); col += output.stride) {
                const int value =
                    maskValues[static_cast<qsizetype>(row)
                               * mask.width() + col];
                if (value != 0)
                    output.points.append(QPoint(col, row));
            }
        }
        totalPoints += output.points.size();
    }

    const bool largeGrid = totalPoints > 100000;
    if (largeGrid) {
        const QString message = outputs.size() == 1
            ? QStringLiteral("This will create %1 points. Continue?")
                  .arg(totalPoints)
            : QStringLiteral(
                  "This will create %1 grid CSV files with %2 total points. Continue?")
                  .arg(outputs.size())
                  .arg(totalPoints);
        const auto answer = QMessageBox::question(
            this,
            "Create grid",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    QString saveError;
    QSet<QString> refreshedDirectories;
    for (const GridOutput &output : std::as_const(outputs)) {
        PointCollection collection =
            PointCollection::createEmpty({"row", "col"});
        for (const QPoint &point : output.points)
            collection.addPoint(point.y() + 0.5, point.x() + 0.5);

        if (!collection.save(output.savePath, &saveError)) {
            QMessageBox::warning(this, "Create grid", saveError);
            return;
        }
        refreshedDirectories.insert(QFileInfo(output.savePath).absolutePath());
    }

    statusBar()->showMessage(
        outputs.size() == 1
            ? QStringLiteral("Created grid points file: %1 (%2 points)")
                  .arg(QFileInfo(outputs.first().savePath).fileName())
                  .arg(outputs.first().points.size())
            : QStringLiteral("Created %1 grid point CSV files (%2 total points)")
                  .arg(outputs.size())
                  .arg(totalPoints),
        5000);
    if (m_projectFileSystemModel) {
        for (const QString &directory : std::as_const(refreshedDirectories))
            m_projectFileSystemModel->refreshDirectory(directory);
    }

    ImageViewer *viewer = currentImageViewer();
    if (outputs.size() == 1 && viewer && viewer->layerManager()) {
        const auto reply = QMessageBox::question(
            this,
            "Add layer",
            "Add the new grid points file as a layer to the current viewer?",
            QMessageBox::Yes | QMessageBox::No,
            largeGrid ? QMessageBox::No : QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            if (PointLayer *layer = createStyledPointLayer(
                    outputs.first().savePath, viewer)) {
                viewer->layerManager()->addLayer(layer);
            }
        }
    }
}

void MainWindow::selectPointsByMask()
{
    if (!m_project.isValid()) {
        QMessageBox::information(
            this, "Select by mask",
            "No project is open. Please create or open a project first.");
        return;
    }

    ImageViewer *viewer = currentImageViewer();
    if (!viewer) {
        QMessageBox::information(
            this, "Select by mask",
            "Open a TIFF viewer first.\n\n"
            "Then add or load a registered point CSV layer and select it in the Layers panel.");
        return;
    }

    LayerManager *manager = viewer ? viewer->layerManager() : nullptr;
    if (!manager) {
        QMessageBox::information(
            this, "Select by mask",
            "The active viewer does not have a layer manager.");
        return;
    }

    bool hasPointLayer = false;
    for (int i = 0; i < manager->layerCount(); ++i) {
        if (qobject_cast<PointLayer*>(manager->layerAt(i))) {
            hasPointLayer = true;
            break;
        }
    }

    auto *pointLayer =
        qobject_cast<PointLayer*>(manager->activeLayer());
    if (!pointLayer) {
        const QString message = hasPointLayer
            ? QStringLiteral(
                "Select a point layer in the Layers panel first.\n\n"
                "Project Explorer selection is not used by this command.")
            : QStringLiteral(
                "No point layer is open in the active viewer.\n\n"
                "Open a TIFF, then add or load a registered point CSV layer before using Select by mask.");
        QMessageBox::information(
            this, "Select by mask",
            message);
        return;
    }

    const Collection pointCollection =
        collectionForPath(pointLayer->sourcePath());
    if (pointCollection.collectionClass != "point_collection") {
        QMessageBox::information(
            this, "Select by mask",
            "Select by mask requires a point layer from a registered point collection.");
        return;
    }

    if (normalizedProjectReference(
            m_project.rootPath(), pointCollection.parent).isEmpty()
        && normalizedProjectReference(
            m_project.rootPath(), pointCollection.related).isEmpty()) {
        QMessageBox::information(
            this, "Select by mask",
            "The point collection does not have a valid parent image collection or related segmentation collection.");
        return;
    }

    const QString pointBaseName =
        QFileInfo(pointLayer->sourcePath()).completeBaseName();
    const QVector<Collection> collections =
        Project::discoverCollections(m_project.rootPath());
    QVector<MaskCandidate> candidates = compatibleMaskCandidates(
        m_project.rootPath(),
        pointCollection,
        collections,
        pointBaseName);

    if (candidates.isEmpty()) {
        QMessageBox::information(
            this, "Select by mask",
            QStringLiteral(
                "No compatible mask was found for %1.\n\n"
                "A compatible mask must be a TIFF with the same basename in a "
                "related segmentation collection or in a segmentation collection "
                "linked to the same parent image collection as the point collection.")
                .arg(QFileInfo(pointLayer->sourcePath()).fileName()));
        return;
    }

    int selectedCandidate = 0;
    if (candidates.size() > 1) {
        QStringList labels;
        labels.reserve(candidates.size());
        for (const MaskCandidate &candidate : std::as_const(candidates))
            labels.append(candidate.label);

        bool ok = false;
        const QString selectedLabel = QInputDialog::getItem(
            this,
            "Select by mask",
            "Mask:",
            labels,
            0,
            false,
            &ok);
        if (!ok)
            return;
        selectedCandidate = labels.indexOf(selectedLabel);
        if (selectedCandidate < 0)
            return;
    }

    const MaskCandidate candidate = candidates.at(selectedCandidate);
    QString errorMessage;
    const RasterData mask = TiffReader::load(candidate.path, &errorMessage);
    if (mask.isNull()) {
        QMessageBox::warning(
            this, "Select by mask",
            QStringLiteral("Could not read mask TIFF:\n%1")
                .arg(errorMessage));
        return;
    }
    if (mask.bands() != 1
        || mask.dataType() != RasterData::DataType::UInt8) {
        QMessageBox::warning(
            this, "Select by mask",
            "The selected mask must be a single-band UInt8 TIFF.");
        return;
    }

    const quint8 *maskValues = mask.bandPointer<quint8>(0);
    if (!maskValues) {
        QMessageBox::warning(
            this, "Select by mask",
            "The selected mask could not be inspected.");
        return;
    }

    const qsizetype pixelCount =
        static_cast<qsizetype>(mask.width()) * mask.height();
    for (qsizetype offset = 0; offset < pixelCount; ++offset) {
        const int value = maskValues[offset];
        if (value != 0 && value != 1 && value != 255) {
            QMessageBox::warning(
                this, "Select by mask",
                "The selected mask is not binary.\n\n"
                "Binary masks must contain only 0/255 values, "
                "or legacy 0/1 values.");
            return;
        }
    }

    QSet<int> selectedIndices;
    const QVector<PointRecord> &points = pointLayer->collection().points();
    for (int index = 0; index < points.size(); ++index) {
        const PointRecord &point = points.at(index);
        const int row = static_cast<int>(std::llround(point.row));
        const int col = static_cast<int>(std::llround(point.col));
        if (row < 0 || row >= mask.height()
            || col < 0 || col >= mask.width()) {
            continue;
        }
        const int value =
            maskValues[static_cast<qsizetype>(row) * mask.width() + col];
        if (value != 0)
            selectedIndices.insert(index);
    }

    pointLayer->setSelectedPointIndices(
        selectedIndices,
        selectedIndices.isEmpty() ? -1 : *selectedIndices.constBegin());
    statusBar()->showMessage(
        QStringLiteral("Selected %1 point%2 using mask %3")
            .arg(selectedIndices.size())
            .arg(selectedIndices.size() == 1 ? "" : "s")
            .arg(QFileInfo(candidate.path).fileName()),
        4000);
}

void MainWindow::configurePointFastPreviewLimit()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Fast preview limit");
    dialog.setMinimumWidth(520);

    auto *layout = new QVBoxLayout(&dialog);

    auto *intro = new QLabel(
        "Point layers with this many points or more are shown as a fast preview. "
        "The CSV remains complete, but interactive point editing and box "
        "selection are disabled to keep pan and zoom responsive.",
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    auto *limitSpin = new QSpinBox(&dialog);
    limitSpin->setRange(AppPreferences::minimumPointFastPreviewLimit(),
                        AppPreferences::maximumPointFastPreviewLimit());
    limitSpin->setSingleStep(1000);
    limitSpin->setValue(AppPreferences::pointFastPreviewLimit());
    limitSpin->setSuffix(" points");
    form->addRow("Fast preview from:", limitSpin);
    layout->addLayout(form);

    auto *note = new QLabel(
        "This is a local application preference, not project metadata. "
        "It applies when point layers are opened or reloaded.",
        &dialog);
    note->setWordWrap(true);
    note->setStyleSheet("color: #6b7280;");
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    auto *resetButton = buttons->addButton(
        "Reset recommended",
        QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(resetButton, &QPushButton::clicked, &dialog, [limitSpin]() {
        limitSpin->setValue(AppPreferences::defaultPointFastPreviewLimit());
    });
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    AppPreferences::setPointFastPreviewLimit(limitSpin->value());
    statusBar()->showMessage(
        QStringLiteral("Point fast preview limit set to %1 points")
            .arg(AppPreferences::pointFastPreviewLimit()),
        4000);
}

void MainWindow::createMaskTiff()
{
    createMaskTiffInCollection();
}

void MainWindow::createMaskTiffInCollection(
    const Collection &initialCollection)
{
    if (!m_project.isValid()) {
        QMessageBox::information(this, "Create mask",
                                 "No project is open. Please create or open a project first.");
        return;
    }

    const QVector<Collection> collections = Project::discoverCollections(m_project.rootPath());
    QVector<Collection> segmentationCollections;
    for (const Collection &col : collections) {
        if (col.collectionClass == "segmentation_collection")
            segmentationCollections.append(col);
    }

    if (segmentationCollections.isEmpty()) {
        QMessageBox::information(this, "Create mask",
                                 "No segmentation collection is registered in this project.\n\n"
                                 "A segmentation collection is a folder registered as a segmentation collection.\n\n"
                                 "To register a folder as a segmentation collection, use Project > Register collection...\n\n"
                                 "If you need an empty folder, create one in Project Explorer with the New Folder button.");
        return;
    }

    Collection selectedCollection = initialCollection;
    if (selectedCollection.collectionClass != "segmentation_collection")
        selectedCollection = collectionForPath(selectedProjectPath());
    if (selectedCollection.collectionClass != "segmentation_collection")
        selectedCollection = {};

    if (selectedCollection.collectionClass != "segmentation_collection"
        && segmentationCollections.size() == 1) {
        selectedCollection = segmentationCollections.first();
    } else if (selectedCollection.collectionClass != "segmentation_collection") {
        QStringList collectionNames;
        for (const Collection &collection : segmentationCollections)
            collectionNames.append(collection.dirName);
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select segmentation collection",
                                                     "Segmentation collection:", collectionNames, 0, false, &ok);
        if (!ok || choice.isEmpty())
            return;
        for (const Collection &collection : segmentationCollections) {
            if (collection.dirName == choice) {
                selectedCollection = collection;
                break;
            }
        }
    }

    const QString selectedDir = selectedCollection.dirPath;
    const QString parentPath = Project::resolveReference(
        m_project.rootPath(), selectedCollection.parent);
    QDir parentDir(parentPath);
    if (!parentDir.exists()) {
        QMessageBox::warning(this, "Create mask",
                             "The selected segmentation collection does not have a valid parent folder.");
        return;
    }

    QSet<QString> usedBaseNames;
    const QFileInfoList existingMasks = QDir(selectedDir).entryInfoList(
        {"*.tif", "*.tiff", "*.TIF", "*.TIFF"}, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &file : existingMasks)
        usedBaseNames.insert(file.completeBaseName().toCaseFolded());

    QMap<QString, QString> parentTiffsByBaseName;
    QStringList ambiguousParentBaseNames;
    const QFileInfoList parentImages = parentDir.entryInfoList(
        {"*.tif", "*.tiff", "*.TIF", "*.TIFF"},
        QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &image : parentImages) {
        const QString baseName = image.completeBaseName();
        const QString foldedBaseName = baseName.toCaseFolded();
        if (parentTiffsByBaseName.contains(foldedBaseName)) {
            if (!ambiguousParentBaseNames.contains(baseName, Qt::CaseInsensitive))
                ambiguousParentBaseNames.append(baseName);
            parentTiffsByBaseName[foldedBaseName] = QString();
        } else {
            parentTiffsByBaseName.insert(foldedBaseName, image.absoluteFilePath());
        }
    }

    QStringList availableBaseNames;
    for (const QFileInfo &image : parentImages) {
        const QString baseName = image.completeBaseName();
        const QString foldedBaseName = baseName.toCaseFolded();
        if (!usedBaseNames.contains(foldedBaseName)
            && !parentTiffsByBaseName.value(foldedBaseName).isEmpty()
            && !availableBaseNames.contains(baseName, Qt::CaseInsensitive)) {
            availableBaseNames.append(baseName);
        }
    }

    PointCsvCreationDialog::Options maskDialogOptions;
    maskDialogOptions.windowTitle = QStringLiteral("Create mask TIFF");
    maskDialogOptions.description =
        QStringLiteral("Select image basenames from the parent collection. "
                       "Empty binary mask TIFF files will be created in the selected segmentation collection.");
    maskDialogOptions.customWindowTitle = QStringLiteral("Custom mask TIFF");
    maskDialogOptions.customPrompt = QStringLiteral("TIFF basename:");
    maskDialogOptions.summarySingular =
        QStringLiteral("1 mask TIFF file will be created.");
    maskDialogOptions.summaryPlural =
        QStringLiteral("%1 mask TIFF files will be created.");
    maskDialogOptions.customSuffixes = {
        QStringLiteral("tif"),
        QStringLiteral("tiff")
    };
    maskDialogOptions.allowCustomName = false;

    const QString foldedCollectionBaseName =
        selectedCollection.dirName.toCaseFolded();
    const QString collectionBaseName =
        !usedBaseNames.contains(foldedCollectionBaseName)
            && !parentTiffsByBaseName.value(
                    foldedCollectionBaseName).isEmpty()
        ? selectedCollection.dirName
        : QString();

    PointCsvCreationDialog dialog(
        maskDialogOptions, collectionBaseName, availableBaseNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedBaseNames = dialog.selectedBaseNames();
    if (selectedBaseNames.isEmpty())
        return;

    QStringList savePaths;
    QStringList parentImagePaths;
    savePaths.reserve(selectedBaseNames.size());
    parentImagePaths.reserve(selectedBaseNames.size());

    for (const QString &baseName : selectedBaseNames) {
        const QString savePath = QDir(selectedDir).filePath(baseName + ".tif");
        if (QFileInfo::exists(savePath)) {
            QMessageBox::warning(
                this,
                "Create mask",
                QStringLiteral("Mask file already exists:\n%1")
                    .arg(QDir::toNativeSeparators(savePath)));
            return;
        }

        const QString foldedBaseName = baseName.toCaseFolded();
        const QString parentImagePath =
            parentTiffsByBaseName.value(foldedBaseName);
        if (parentImagePath.isEmpty()) {
            const bool ambiguous = std::any_of(
                ambiguousParentBaseNames.cbegin(),
                ambiguousParentBaseNames.cend(),
                [&baseName](const QString &ambiguousBaseName) {
                    return ambiguousBaseName.compare(baseName,
                                            Qt::CaseInsensitive) == 0;
                });
            QMessageBox::warning(
                this, "Create mask",
                ambiguous
                    ? QStringLiteral("The parent collection contains multiple TIFF files named \"%1\". "
                                     "Use a unique basename before creating a mask.")
                          .arg(baseName)
                    : QStringLiteral("No parent TIFF named \"%1\" was found. "
                                     "The mask name must match a TIFF in the parent collection.")
                          .arg(baseName));
            return;
        }

        savePaths.append(savePath);
        parentImagePaths.append(parentImagePath);
    }

    QString errorMsg;
    for (qsizetype i = 0; i < savePaths.size(); ++i) {
        const RasterData parentRaster =
            TiffReader::load(parentImagePaths.at(i), &errorMsg);
        if (parentRaster.isNull()) {
            QMessageBox::warning(this, "Create mask",
                                 errorMsg.isEmpty()
                                     ? "Could not read the parent TIFF."
                                     : errorMsg);
            return;
        }

        RasterData maskRaster;
        maskRaster.allocate(parentRaster.width(), parentRaster.height(),
                            1, RasterData::DataType::UInt8);
        if (!TiffWriter::write(savePaths.at(i), maskRaster, &errorMsg)) {
            QMessageBox::warning(this, "Create mask", errorMsg);
            return;
        }
    }

    statusBar()->showMessage(
        savePaths.size() == 1
            ? QStringLiteral("Created mask file: %1")
                  .arg(QFileInfo(savePaths.first()).fileName())
            : QStringLiteral("Created %1 mask TIFF files")
                  .arg(savePaths.size()),
        5000);
    m_projectFileSystemModel->refreshDirectory(selectedDir);

    ImageViewer *viewer = currentImageViewer();
    if (savePaths.size() == 1 && viewer && viewer->layerManager()) {
        const auto reply = QMessageBox::question(this, "Add layer",
                                                 "Add the new mask file as a layer to the current viewer?");
        if (reply == QMessageBox::Yes) {
            const CollectionStyleData existingStyle =
                CollectionStyleFile::load(selectedCollection.dirPath);
            if (existingStyle.kind != CollectionStyleData::Kind::Image) {
                const auto styleReply = QMessageBox::question(
                    this,
                    "Mask style",
                    "This mask collection does not have a saved style.\n\n"
                    "Create a default mask style for this collection?",
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);
                if (styleReply == QMessageBox::Yes) {
                    CollectionStyleData maskStyle;
                    maskStyle.kind = CollectionStyleData::Kind::Image;
                    maskStyle.imageStyle.opacity = 0.5;
                    maskStyle.imageStyle.blendMode = ImageBlendMode::Multiply;
                    maskStyle.imageStyle.render.mode =
                        RasterRenderMode::SingleBand;
                    maskStyle.imageStyle.render.colorRamp =
                        ColorRamp::RedBlue;
                    maskStyle.imageStyle.render.stretchMode =
                        StretchMode::None;
                    maskStyle.imageStyle.render.tailTrim = false;
                    maskStyle.imageStyle.render.manualRangeEnabled = false;

                    QString styleError;
                    if (!CollectionStyleFile::save(
                            selectedCollection.dirPath,
                            maskStyle,
                            &styleError)) {
                        QMessageBox::warning(
                            this,
                            "Mask style",
                            styleError.isEmpty()
                                ? "The default mask style could not be saved."
                                : styleError);
                    } else {
                        m_projectFileSystemModel->refreshDirectory(
                            selectedCollection.dirPath);
                    }
                }
            }
            if (ImageLayer *layer = createStyledImageLayer(savePaths.first()))
                viewer->layerManager()->addLayer(layer);
        }
    }
}
