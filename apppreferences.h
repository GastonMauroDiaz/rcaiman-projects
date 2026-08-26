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

#ifndef APPPREFERENCES_H
#define APPPREFERENCES_H

#include <QByteArray>
#include <QStringList>

class AppPreferences
{
public:
    static int projectExplorerSortMode(int fallback);
    static void setProjectExplorerSortMode(int mode);

    static QStringList recentProjects();
    static void setRecentProjects(const QStringList &paths);

    static QByteArray mainWindowGeometry();
    static void setMainWindowGeometry(const QByteArray &geometry);

    static QByteArray mainWindowState();
    static void setMainWindowState(const QByteArray &state);

    static int defaultPointFastPreviewLimit();
    static int minimumPointFastPreviewLimit();
    static int maximumPointFastPreviewLimit();

    static int pointFastPreviewLimit();
    static void setPointFastPreviewLimit(int limit);
    static void resetPointFastPreviewLimit();

private:
    AppPreferences() = delete;
};

#endif // APPPREFERENCES_H
