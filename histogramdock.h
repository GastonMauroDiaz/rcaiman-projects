#ifndef HISTOGRAMDOCK_H
#define HISTOGRAMDOCK_H

#include <QColor>
#include <QDockWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class ImageLayer;

class HistogramWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget *parent = nullptr);

    void setLayer(ImageLayer *layer);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class Handle {
        None,
        Minimum,
        Maximum
    };

    struct Series {
        QString label;
        QColor color;
        QVector<quint64> bins;
        int band = 0;
        double rawMin = 0.0;
        double rawMax = 1.0;
        double renderMin = 0.0;
        double renderMax = 1.0;
    };

    void rebuild();
    void addSeries(const QString &label, const QColor &color, int band);
    void rebuildBins();
    QString summaryText() const;
    QRect plotRectForSummary(const QString &summary) const;
    Handle handleAtPosition(const QPoint &position) const;
    double valueForX(int x) const;
    int xForValue(double value) const;
    void updateCursorForPosition(const QPoint &position);
    void applyHandleDrag(int x);

    QPointer<ImageLayer> m_layer;
    QVector<Series> m_series;
    Handle m_dragHandle = Handle::None;
    double m_displayMin = 0.0;
    double m_displayMax = 1.0;
};

class HistogramDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit HistogramDock(QWidget *parent = nullptr);

    void setLayer(ImageLayer *layer);

private:
    HistogramWidget *m_widget = nullptr;
};

#endif // HISTOGRAMDOCK_H
