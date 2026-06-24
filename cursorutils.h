#ifndef CURSORUTILS_H
#define CURSORUTILS_H

#include <QCursor>
#include <QPoint>
#include <QString>

QCursor highDpiCursor(const QString &resourcePath,
                      const QPoint &logicalHotspot,
                      Qt::CursorShape fallback);

#endif // CURSORUTILS_H
