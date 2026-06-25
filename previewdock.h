#ifndef PREVIEWDOCK_H
#define PREVIEWDOCK_H

#include <QDockWidget>

class QStackedWidget;
class QTextBrowser;
class QLabel;

class PreviewDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit PreviewDock(QWidget *parent = nullptr);

    void showText(const QString &text);
    void showMarkdown(const QString &markdown);
    void showImage(const QPixmap &pixmap);
    void clear();

private:
    QStackedWidget *m_stack = nullptr;
    QTextBrowser *m_textEdit = nullptr;
    QLabel *m_imageLabel = nullptr;
};

#endif // PREVIEWDOCK_H
