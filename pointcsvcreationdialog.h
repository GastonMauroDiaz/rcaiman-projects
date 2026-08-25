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

#ifndef POINTCSVCREATIONDIALOG_H
#define POINTCSVCREATIONDIALOG_H

#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QListWidget;

class PointCsvCreationDialog : public QDialog
{
    Q_OBJECT

public:
    struct Options {
        QString windowTitle = QStringLiteral("Create points CSV");
        QString description =
            QStringLiteral("Select image basenames from the parent collection. "
                           "Empty point CSV files will be created in the selected point collection.");
        QString customWindowTitle = QStringLiteral("Custom points CSV");
        QString customPrompt = QStringLiteral("CSV basename:");
        QString duplicateMessage =
            QStringLiteral("That basename is already selected.");
        QString summarySingular =
            QStringLiteral("1 point CSV file will be created.");
        QString summaryPlural =
            QStringLiteral("%1 point CSV files will be created.");
        QStringList customSuffixes = {QStringLiteral("csv")};
        bool allowCustomName = true;
    };

    explicit PointCsvCreationDialog(const QString &collectionName,
                                    const QStringList &availableBaseNames,
                                    QWidget *parent = nullptr);
    explicit PointCsvCreationDialog(const Options &options,
                                    const QString &collectionName,
                                    const QStringList &availableBaseNames,
                                    QWidget *parent = nullptr);

    QStringList selectedBaseNames() const;

private:
    void addSelected();
    void removeSelected();
    void addAll();
    void clearSelected();
    void addCustomName();
    void updateSummary();
    bool addBaseNameToSelected(const QString &baseName);
    bool selectedContains(const QString &baseName) const;
    void addAvailableBaseName(const QString &baseName);
    void addCustomItem();

    QListWidget *m_availableList = nullptr;
    QListWidget *m_selectedList = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    Options m_options;
};

#endif // POINTCSVCREATIONDIALOG_H
