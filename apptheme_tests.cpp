// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#include "apptheme.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QPalette palette = AppTheme::lightPalette();

    if (palette.color(QPalette::Window) != QColor("#f0f0f0")
        || palette.color(QPalette::Base) != QColor("#ffffff")
        || palette.color(QPalette::Text) != QColor("#202124")) {
        return 1;
    }

    if (palette.color(QPalette::Active, QPalette::Highlight)
            != QColor("#0078d4")
        || palette.color(QPalette::Active, QPalette::HighlightedText)
            != QColor("#ffffff")) {
        return 2;
    }

    if (palette.color(QPalette::Inactive, QPalette::Highlight)
            != QColor("#d9eafa")
        || palette.color(QPalette::Inactive, QPalette::HighlightedText)
            != QColor("#202124")) {
        return 3;
    }

    if (palette.color(QPalette::Disabled, QPalette::Text)
            != QColor("#858585")) {
        return 4;
    }

    return 0;
}
