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

#ifndef LAYER_H
#define LAYER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QGraphicsItem>
#include <QTimer>

class QGraphicsScene;

class Layer : public QObject
{
    Q_OBJECT

public:
    explicit Layer(const QString &name, const QString &sourcePath,
                   QObject *parent = nullptr);
    ~Layer() override;

    QString name() const;
    void rename(const QString &name);
    QString sourcePath() const;

    bool isVisible() const;
    void setVisible(bool visible);
    bool isActive() const;
    virtual void setActive(bool active);
    void setZOrder(int zOrder);

    int zOrder() const;        // calculado desde el índice en el manager

    // Construye y cachea los items gráficos.
    // Se llama una vez: en addLayer() o en el primer build necesario.
    virtual const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) = 0;
    virtual bool supportsReloadFromDisk() const;
    virtual bool reloadFromDisk(QString *errorMessage = nullptr);

    // Agrega los items cacheados a la escena (o los oculta según visibilidad)
    void addToScene(QGraphicsScene *scene);
    // Remueve items de la escena pero no los destruye (los conserva cacheados)
    void removeFromScene(QGraphicsScene *scene);
    // Libera items cacheados (para recarga completa)
    void clearCachedItems();

    // Flicker
    void setFlickerEnabled(bool enabled);
    bool flickerEnabled() const;
    void setFlickerInterval(double seconds);

    QVector<QGraphicsItem*> m_items;  // cacheados; no se recrean en cada cambio
    bool m_itemsBuilt = false;

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);
    void flickerChanged(bool enabled);
    void styleChanged();
    void dataChanged();

private slots:
    void onFlickerTick();

private:
    QString m_name;
    QString m_sourcePath;
    bool m_visible = true;
    bool m_active = false;
    int m_zOrder = 0;
    bool m_flickerEnabled = false;
    bool m_visibleBeforeFlicker = true;
    QTimer *m_flickerTimer = nullptr;
    double m_flickerIntervalSec = 0.5;
};

#endif // LAYER_H
