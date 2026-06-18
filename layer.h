#ifndef LAYER_H
#define LAYER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QGraphicsItem>

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

    int zOrder() const;        // calculado desde el índice en el manager

    // Construye y cachea los items gráficos.
    // Se llama una vez: en addLayer() o en el primer build necesario.
    virtual const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) = 0;

    // Agrega los items cacheados a la escena (o los oculta según visibilidad)
    void addToScene(QGraphicsScene *scene);
    // Remueve items de la escena pero no los destruye (los conserva cacheados)
    void removeFromScene(QGraphicsScene *scene);
    // Libera items cacheados (para recarga completa)
    void clearCachedItems();

    QVector<QGraphicsItem*> m_items;  // cacheados; no se recrean en cada cambio
    bool m_itemsBuilt = false;

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);

private:
    QString m_name;
    QString m_sourcePath;
    bool m_visible = true;
};

#endif // LAYER_H