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

#include "apppreferences.h"

#include <QSettings>
#include <QtGlobal>

namespace {
constexpr int DefaultPointFastPreviewLimit = 20000;
constexpr int MinimumPointFastPreviewLimit = 1000;
constexpr int MaximumPointFastPreviewLimit = 10000000;

const char *projectExplorerSortModeKey()
{
    return "projectExplorer/sortMode";
}

const char *recentProjectsKey()
{
    return "recentProjects";
}

const char *mainWindowGeometryKey()
{
    return "mainWindow/geometry";
}

const char *mainWindowStateKey()
{
    return "mainWindow/state";
}

const char *pointFastPreviewLimitKey()
{
    // Local user preference, not project metadata. Keep this class as the
    // growth point for future per-installation settings.
    return "performance/pointFastPreviewLimit";
}

int normalizePointFastPreviewLimit(int limit)
{
    return qBound(MinimumPointFastPreviewLimit,
                  limit,
                  MaximumPointFastPreviewLimit);
}
}

int AppPreferences::projectExplorerSortMode(int fallback)
{
    QSettings settings;
    return settings.value(projectExplorerSortModeKey(), fallback).toInt();
}

void AppPreferences::setProjectExplorerSortMode(int mode)
{
    QSettings settings;
    settings.setValue(projectExplorerSortModeKey(), mode);
}

QStringList AppPreferences::recentProjects()
{
    QSettings settings;
    return settings.value(recentProjectsKey()).toStringList();
}

void AppPreferences::setRecentProjects(const QStringList &paths)
{
    QSettings settings;
    settings.setValue(recentProjectsKey(), paths);
}

QByteArray AppPreferences::mainWindowGeometry()
{
    QSettings settings;
    return settings.value(mainWindowGeometryKey()).toByteArray();
}

void AppPreferences::setMainWindowGeometry(const QByteArray &geometry)
{
    QSettings settings;
    settings.setValue(mainWindowGeometryKey(), geometry);
}

QByteArray AppPreferences::mainWindowState()
{
    QSettings settings;
    return settings.value(mainWindowStateKey()).toByteArray();
}

void AppPreferences::setMainWindowState(const QByteArray &state)
{
    QSettings settings;
    settings.setValue(mainWindowStateKey(), state);
}

int AppPreferences::defaultPointFastPreviewLimit()
{
    return DefaultPointFastPreviewLimit;
}

int AppPreferences::minimumPointFastPreviewLimit()
{
    return MinimumPointFastPreviewLimit;
}

int AppPreferences::maximumPointFastPreviewLimit()
{
    return MaximumPointFastPreviewLimit;
}

int AppPreferences::pointFastPreviewLimit()
{
    QSettings settings;
    return normalizePointFastPreviewLimit(
        settings.value(pointFastPreviewLimitKey(),
                       DefaultPointFastPreviewLimit).toInt());
}

void AppPreferences::setPointFastPreviewLimit(int limit)
{
    QSettings settings;
    settings.setValue(pointFastPreviewLimitKey(),
                      normalizePointFastPreviewLimit(limit));
}

void AppPreferences::resetPointFastPreviewLimit()
{
    QSettings settings;
    settings.remove(pointFastPreviewLimitKey());
}
