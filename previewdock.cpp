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

#include "previewdock.h"

#include <QLabel>
#include <QResizeEvent>
#include <QTextBrowser>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

PreviewDock::PreviewDock(QWidget *parent)
    : QDockWidget("Preview", parent)
{
    setObjectName("PreviewDock");
    setWindowTitle("Preview");

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    // Page 0: text
    m_textEdit = new QTextBrowser(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setOpenExternalLinks(true);
    m_stack->addWidget(m_textEdit);  // index 0

    // Page 1: image
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel(scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    scrollArea->setWidget(m_imageLabel);
    m_stack->addWidget(scrollArea);  // index 1

    layout->addWidget(m_stack);
    setWidget(container);

    clear();
}

void PreviewDock::showText(const QString &text)
{
    m_stack->setCurrentIndex(0);
    m_textEdit->setPlainText(text);
}

void PreviewDock::showMarkdown(const QString &markdown)
{
    m_stack->setCurrentIndex(0);
    m_textEdit->setMarkdown(markdown);
}

void PreviewDock::showImage(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        clear();
        return;
    }

    m_stack->setCurrentIndex(1);
    m_originalPixmap = pixmap;
    updateImagePreview();
}

void PreviewDock::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    if (m_stack && m_stack->currentIndex() == 1)
        updateImagePreview();
}

void PreviewDock::updateImagePreview()
{
    if (m_originalPixmap.isNull() || !m_imageLabel)
        return;

    const int dockWidth = width() - 20; // some margin
    if (dockWidth > 0 && m_originalPixmap.width() > dockWidth) {
        m_imageLabel->setPixmap(
            m_originalPixmap.scaledToWidth(
                dockWidth, Qt::SmoothTransformation));
    } else {
        m_imageLabel->setPixmap(m_originalPixmap);
    }
}

void PreviewDock::clear()
{
    m_textEdit->clear();
    m_imageLabel->clear();
    m_originalPixmap = QPixmap();
    m_stack->setCurrentIndex(0);
}
