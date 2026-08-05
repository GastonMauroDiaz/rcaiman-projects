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

#ifndef HSPPROJECTIMPORTER_H
#define HSPPROJECTIMPORTER_H

#include "project.h"

#include <QString>
#include <QStringList>

struct HspImportOptions
{
    QString sourceProjectPath;
    QString destinationProjectPath;
    QString projectName;
    QString description;
};

struct HspImportReport
{
    int rawFilesCopied = 0;
    int pgmFilesConverted = 0;
    int skyPointFilesWritten = 0;
    int sunMarkFilesWritten = 0;
    int maskFilesWritten = 0;
    int markedAreaFilesWritten = 0;
    int optSkyCoefTablesWritten = 0;
    QStringList groups;
    QStringList warnings;
    QStringList omitted;
};

class HspProjectImporter
{
public:
    static Project importProject(const HspImportOptions &options,
                                 HspImportReport *report = nullptr,
                                 QString *errorMessage = nullptr);
};

#endif // HSPPROJECTIMPORTER_H
