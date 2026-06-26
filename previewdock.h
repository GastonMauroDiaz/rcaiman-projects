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
