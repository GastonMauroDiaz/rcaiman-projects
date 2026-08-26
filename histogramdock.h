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

signals:
    void computationStarted(const QString &message);
    void computationFinished(const QString &message);

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
        QVector<double> sortedSamples;
        int band = 0;
        double rawMin = 0.0;
        double rawMax = 1.0;
        double renderMin = 0.0;
        double renderMax = 1.0;
    };

    struct CacheKey {
        QPointer<ImageLayer> layer;
        quint64 dataRevision = 0;
        int mode = 0;
        int band0 = -1;
        int band1 = -1;
        int band2 = -1;
    };

    struct CacheEntry {
        CacheKey key;
        QVector<Series> series;
        double displayMin = 0.0;
        double displayMax = 1.0;
    };

    void rebuild();
    void rebuildNow();
    void addSeries(QVector<Series> *series,
                   const QString &label,
                   const QColor &color,
                   int band);
    void rebuildBins(QVector<Series> *series,
                     double *displayMin,
                     double *displayMax) const;
    void updateRenderRanges(QVector<Series> *series) const;
    CacheKey currentCacheKey() const;
    bool cacheKeyMatches(const CacheKey &left, const CacheKey &right) const;
    bool loadFromCache(const CacheKey &key);
    void storeInCache(const CacheKey &key,
                      const QVector<Series> &series,
                      double displayMin,
                      double displayMax);
    void trimCache();
    void setComputing(bool computing);
    void updateRenderRanges();
    QString summaryText() const;
    QRect plotRectForSummary(const QString &summary) const;
    Handle handleAtPosition(const QPoint &position) const;
    double valueForX(int x) const;
    int xForValue(double value) const;
    void updateCursorForPosition(const QPoint &position);
    void applyHandleDrag(int x);

    QPointer<ImageLayer> m_layer;
    QVector<Series> m_series;
    QVector<CacheEntry> m_cache;
    Handle m_dragHandle = Handle::None;
    double m_displayMin = 0.0;
    double m_displayMax = 1.0;
    bool m_computing = false;
    bool m_rebuildQueued = false;
};

class HistogramDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit HistogramDock(QWidget *parent = nullptr);

    void setLayer(ImageLayer *layer);

signals:
    void computationStarted(const QString &message);
    void computationFinished(const QString &message);

private:
    HistogramWidget *m_widget = nullptr;
};

#endif // HISTOGRAMDOCK_H
