#include "cursorutils.h"

#include <QPixmap>
#include <QSize>

QCursor highDpiCursor(const QString &resourcePath,
                      const QPoint &logicalHotspot,
                      Qt::CursorShape fallback)
{
    constexpr int logicalSize = 32;
    constexpr qreal sourceDevicePixelRatio = 2.0;
    const QSize sourceSize(
        qRound(logicalSize * sourceDevicePixelRatio),
        qRound(logicalSize * sourceDevicePixelRatio));

    QPixmap pixmap(resourcePath);
    if (pixmap.isNull())
        return QCursor(fallback);

    if (pixmap.size() != sourceSize) {
        pixmap = pixmap.scaled(
            sourceSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    }

    pixmap.setDevicePixelRatio(sourceDevicePixelRatio);
    return QCursor(pixmap, logicalHotspot.x(), logicalHotspot.y());
}
