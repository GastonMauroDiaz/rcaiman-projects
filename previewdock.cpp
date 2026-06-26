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
