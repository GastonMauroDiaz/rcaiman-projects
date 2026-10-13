// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#include "apptheme.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>
#include <QtGlobal>

QPalette AppTheme::lightPalette()
{
    QPalette palette;

    const QColor window("#f0f0f0");
    const QColor panel("#ffffff");
    const QColor alternate("#f7f7f7");
    const QColor text("#202124");
    const QColor mutedText("#6b7280");
    const QColor button("#f0f0f0");
    const QColor border("#a0a0a0");
    const QColor highlight("#0078d4");
    const QColor inactiveHighlight("#d9eafa");
    const QColor link("#0067c0");

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, panel);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffdc"));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Light, QColor("#ffffff"));
    palette.setColor(QPalette::Midlight, QColor("#e3e3e3"));
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Dark, QColor("#696969"));
    palette.setColor(QPalette::Shadow, QColor("#000000"));
    palette.setColor(QPalette::Link, link);
    palette.setColor(QPalette::LinkVisited, QColor("#6f42c1"));
    palette.setColor(QPalette::PlaceholderText, mutedText);

    palette.setColor(QPalette::Active, QPalette::Highlight, highlight);
    palette.setColor(
        QPalette::Active, QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(
        QPalette::Inactive, QPalette::Highlight, inactiveHighlight);
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, text);

    const QColor disabledText("#858585");
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(
        QPalette::Disabled, QPalette::PlaceholderText, QColor("#a0a0a0"));
    palette.setColor(
        QPalette::Disabled, QPalette::Highlight, QColor("#dddddd"));
    palette.setColor(
        QPalette::Disabled, QPalette::HighlightedText, disabledText);

    return palette;
}

void AppTheme::applyLight(QApplication &application)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
    application.setPalette(lightPalette());
}
