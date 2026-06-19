#ifndef PREVIEWDOCK_H
#define PREVIEWDOCK_H

#include <QDockWidget>

class QStackedWidget;
class QPlainTextEdit;
class QLabel;

class PreviewDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit PreviewDock(QWidget *parent = nullptr);

    void showText(const QString &text);
    void showImage(const QPixmap &pixmap);
    void clear();

private:
    QStackedWidget *m_stack = nullptr;
    QPlainTextEdit *m_textEdit = nullptr;
    QLabel *m_imageLabel = nullptr;
};

#endif // PREVIEWDOCK_H