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

#ifndef PREVIEWDOCK_H
#define PREVIEWDOCK_H

#include <QDockWidget>
#include <QPixmap>

class QStackedWidget;
class QTextBrowser;
class QLabel;
class QResizeEvent;

class PreviewDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit PreviewDock(QWidget *parent = nullptr);

    void showText(const QString &text);
    void showMarkdown(const QString &markdown);
    void showImage(const QPixmap &pixmap);
    void clear();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateImagePreview();

    QStackedWidget *m_stack = nullptr;
    QTextBrowser *m_textEdit = nullptr;
    QLabel *m_imageLabel = nullptr;
    QPixmap m_originalPixmap;
};

#endif // PREVIEWDOCK_H
