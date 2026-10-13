// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copyright (C) 2026 GASTON MAURO DIAZ
//
// Part of the rcaiman suite.

#ifndef APPTHEME_H
#define APPTHEME_H

#include <QPalette>

class QApplication;

namespace AppTheme {

QPalette lightPalette();
void applyLight(QApplication &application);

}

#endif // APPTHEME_H
