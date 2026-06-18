#ifndef OVERLAY_H
#define OVERLAY_H

#include <QVector>
#include <QGraphicsItem>

class QGraphicsScene;

class Overlay
{
public:
    virtual ~Overlay() = default;

    // Agrega los items graficos de este overlay a la escena.
    // La escena es propietaria de los items una vez agregados.
    virtual void addToScene(QGraphicsScene *scene) const = 0;
};

#endif // OVERLAY_H