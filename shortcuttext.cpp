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

#include "shortcuttext.h"

namespace ShortcutText {

QString primaryModifier()
{
#ifdef Q_OS_MAC
    return QStringLiteral("Command");
#else
    return QStringLiteral("Ctrl");
#endif
}

QString alternateModifier()
{
#ifdef Q_OS_MAC
    return QStringLiteral("Option");
#else
    return QStringLiteral("Alt");
#endif
}

QString panTooltip()
{
    return QStringLiteral(
        "Pan view\n"
        "Hold Space for temporary pan from any interaction mode.");
}

QString zoomTooltip()
{
    return QStringLiteral(
        "Zoom view\n"
        "Hold Z for temporary zoom from any interaction mode.");
}

QString editPointsTooltip()
{
    return QStringLiteral(
        "Edit the active point layer.\n"
        "Click empty space to add, drag a point to move it, and %1+click to delete.")
        .arg(alternateModifier());
}

QString interactionShortcutsSummaryHtml()
{
    return QStringLiteral(
        "Go to <b>Help &gt; Interaction shortcuts...</b> to learn the "
        "mouse and keyboard combinations that are easy to miss."
        "<p>For example: hold <b>Space</b> to pan or <b>Z</b> and drag "
        "to zoom; press <b>9</b> to fit the image and <b>0</b> for "
        "100%.</p>");
}

QString interactionShortcutsMarkdown()
{
    const QString primary = primaryModifier();
    const QString alternate = alternateModifier();
    return QStringLiteral(
        "## Navigation\n\n"
        "- **Pan:** hold `Space` from any interaction mode.\n"
        "- **Zoom:** hold `Z` from any interaction mode, then drag with the left mouse button.\n"
        "- **100% zoom:** press `0` in the viewer.\n"
        "- **Fit to view:** press `9` in the viewer.\n"
        "- **Mouse wheel:** zoom under the cursor.\n\n"
        "## Point Selection\n\n"
        "- **Toggle point:** `%1+click` a point.\n"
        "- **Box selection:** `%1+drag` on empty image space.\n"
        "- **Add box selection:** `%1+Shift+drag` on empty image space.\n\n"
        "## Point Editing\n\n"
        "- **Add point:** click empty image space while editing a point layer.\n"
        "- **Move point:** drag a point while editing its layer.\n"
        "- **Delete point:** `%2+click` a point while editing. If the point is part of a multiple selection, all selected points are deleted.\n\n"
        "## Mask Editing\n\n"
        "- **Paint mask:** drag while editing a mask layer.\n"
        "- **Erase mask:** `%2+drag` while editing a mask layer.\n"
        "- **Straight brush stroke:** `Shift+click` while editing a mask layer.\n"
        "- **Erase straight brush stroke:** `Shift+%2+click` while editing a mask layer.\n"
        "- **Brush size:** press `[` or `]` in the viewer.\n\n"
        "- **Invert connected region:** `%1+double click` while editing a mask layer.\n\n"
        "## Automatic Reload\n\n"
        "- **Images:** opened image layers reload automatically when their files change on disk.\n"
        "- **Points:** opened point CSV layers reload automatically when their files change on disk.\n"
        "- **Point editing:** external changes to a point CSV are ignored while that layer is being edited. During editing, rcaiman-projects keeps its own CSV state and may overwrite external changes.\n")
        .arg(primary, alternate);
}

}
