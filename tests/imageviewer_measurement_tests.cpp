// SPDX-License-Identifier: GPL-3.0-or-later

#include "pointlayer.h"
#include "imageviewer.h"
#include "layer.h"
#include "layermanager.h"

#include <QApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QImage>
#include <QPaintEvent>
#include <QScrollBar>
#include <QSplitter>

#include <cmath>

namespace {
class CanvasLayer : public Layer
{
public:
    CanvasLayer() : Layer("Measurement test", QString()) {}

    const QVector<QGraphicsItem*> &buildItems(QGraphicsScene *scene) override
    {
        if (!m_itemsBuilt) {
            m_items.append(scene->addRect(QRectF(0, 0, 2000, 1500),
                QPen(Qt::NoPen), QBrush(QColor(70, 70, 70))));
            m_itemsBuilt = true;
        }
        return m_items;
    }
};

bool close(double a, double b) { return std::abs(a - b) < 1e-6; }
bool close(const QPointF &a, const QPointF &b)
{
    return QLineF(a, b).length() < 1e-6;
}

void mouse(ImageViewer &viewer, QEvent::Type type, const QPoint &pos,
           Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(pos),
        QPointF(viewer.viewport()->mapToGlobal(pos)), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(viewer.viewport(), &event);
}

void drag(ImageViewer &viewer, const QPointF &from, const QPointF &to)
{
    const QPoint start = viewer.mapFromScene(from);
    const QPoint end = viewer.mapFromScene(to);
    mouse(viewer, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(viewer, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    mouse(viewer, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
}

void key(ImageViewer &viewer, QEvent::Type type, int keyCode)
{
    QKeyEvent event(type, keyCode, Qt::NoModifier);
    QApplication::sendEvent(viewer.viewport(), &event);
}

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical() << message;
    return condition;
}

class PaintTracker : public QObject
{
public:
    QRegion painted;

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Paint)
            painted += static_cast<QPaintEvent*>(event)->region();
        return false;
    }
};

bool panRepaintChecks(ImageViewer &viewer, ImageViewer &sibling)
{
    PaintTracker activePaint, siblingPaint;
    viewer.viewport()->installEventFilter(&activePaint);
    sibling.viewport()->installEventFilter(&siblingPaint);
    const auto flushPaint = [] {
        QApplication::processEvents();
        QApplication::processEvents();
    };
    const bool syncBefore = ImageViewer::syncEnabled();
    const QPoint scrollBefore(viewer.horizontalScrollBar()->value(), viewer.verticalScrollBar()->value());
    const QPoint siblingScrollBefore(sibling.horizontalScrollBar()->value(), sibling.verticalScrollBar()->value());
    bool passed = true;
    for (bool sync : {false, true}) {
        ImageViewer::setSyncEnabled(sync);
        for (bool circle : {false, true}) {
            viewer.setMeasurementCircleVisible(circle);
            sibling.setMeasurementCircleVisible(circle);
            flushPaint();
            for (const QPoint &delta : {QPoint(23, 0), QPoint(0, 19), QPoint(-23, -19)}) {
                activePaint.painted = {};
                siblingPaint.painted = {};
                const QPoint before(viewer.horizontalScrollBar()->value(), viewer.verticalScrollBar()->value());
                viewer.horizontalScrollBar()->setValue(before.x() + delta.x());
                viewer.verticalScrollBar()->setValue(before.y() + delta.y());
                flushPaint();
                passed &= check(viewer.horizontalScrollBar()->value() != before.x()
                                || viewer.verticalScrollBar()->value() != before.y(),
                                "The pan repaint test must actually scroll the viewport");
                passed &= check(QRegion(viewer.viewport()->rect()).subtracted(activePaint.painted).isEmpty(),
                                "Panning must repaint all screen-positioned measurement labels, not leave copied trails");
                if (sync)
                    passed &= check(QRegion(sibling.viewport()->rect()).subtracted(siblingPaint.painted).isEmpty(),
                                    "Synchronized pan must also repaint the sibling's measurement overlay");
            }
        }
    }
    ImageViewer::setSyncEnabled(false);
    viewer.horizontalScrollBar()->setValue(scrollBefore.x());
    viewer.verticalScrollBar()->setValue(scrollBefore.y());
    sibling.horizontalScrollBar()->setValue(siblingScrollBefore.x());
    sibling.verticalScrollBar()->setValue(siblingScrollBefore.y());
    ImageViewer::setSyncEnabled(syncBefore);
    return passed;
}

std::array<QPointF, 3> vertices(const ImageViewer &viewer)
{
    return {viewer.measurementVertex(0), viewer.measurementVertex(1), viewer.measurementVertex(2)};
}

void doubleClick(ImageViewer &viewer, const QPoint &pos)
{
    mouse(viewer, QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton);
    mouse(viewer, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton);
}

QWidget *editor(ImageViewer &viewer)
{
    return viewer.findChild<QWidget*>(QStringLiteral("measurementEditor"));
}

void unlock(ImageViewer &viewer)
{
    for (int vertex = 0; vertex < 3; ++vertex)
        viewer.setMeasurementVertexLocked(vertex, false);
}

bool lockChecks(ImageViewer &viewer)
{
    viewer.setZoomPercent(100);
    const QPointF center = viewer.mapToScene(viewer.viewport()->rect().center());
    const std::array<QPointF, 3> points{
        center + QPointF(-90, 60), center + QPointF(90, 60), center + QPointF(-90, -120)};
    for (int mask = 0; mask < 8; ++mask) {
        unlock(viewer);
        for (int vertex = 0; vertex < 3; ++vertex) {
            viewer.setMeasurementVertex(vertex, points[vertex]);
            viewer.setMeasurementVertexLocked(vertex, mask & (1 << vertex));
        }
        if (!check(viewer.measurementTranslationAllowed() == (mask == 0),
                   "Whole-triangle translation requires all three locks to be open"))
            return false;
        for (int vertex = 0; vertex < 3; ++vertex) {
            const bool locked = mask & (1 << vertex);
            const auto before = vertices(viewer);
            viewer.setMeasurementVertex(vertex, before[vertex] + QPointF(4, 7));
            if (!check(close(viewer.measurementVertex(vertex),
                             locked ? before[vertex] : before[vertex] + QPointF(4, 7)),
                       "A lock must protect a vertex from numeric changes"))
                return false;
            viewer.setMeasurementVertex(vertex, before[vertex]);
        }
        for (int side = 0; side < 3; ++side) {
            const int next = (side + 1) % 3;
            const bool editable = bool(mask & (1 << side)) != bool(mask & (1 << next));
            const auto before = vertices(viewer);
            const double target = viewer.measurementSideLength(side) * 1.25;
            if (!check(viewer.canEditMeasurementSide(side) == editable
                       && viewer.setMeasurementSideLength(side, target) == editable,
                       "Side editing requires exactly one locked endpoint"))
                return false;
            if (editable) {
                const int fixed = mask & (1 << side) ? side : next;
                const int moving = fixed == side ? next : side;
                const QPointF expected = before[fixed] + (before[moving] - before[fixed]) * 1.25;
                if (!check(close(viewer.measurementVertex(fixed), before[fixed])
                           && close(viewer.measurementVertex(moving), expected)
                           && close(viewer.measurementSideLength(side), target)
                           && close(viewer.measurementVertex((side + 2) % 3), before[(side + 2) % 3]),
                           "Length editing must move only the free endpoint and preserve its direction"))
                    return false;
                viewer.setMeasurementVertex(moving, before[moving]);
            } else if (!check(vertices(viewer) == before, "Ineligible length edits must not change any point")) {
                return false;
            }
        }
        const auto before = vertices(viewer);
        const QPointF centroid = (points[0] + points[1] + points[2]) / 3;
        drag(viewer, centroid, centroid + QPointF(12, 8));
        if (mask != 0 && !check(vertices(viewer) == before,
                               "Dragging inside with any closed lock must leave the triangle unchanged"))
            return false;
    }
    unlock(viewer);
    for (int vertex = 0; vertex < 3; ++vertex)
        viewer.setMeasurementVertex(vertex, points[vertex]);

    QPoint padlock(-1, -1);
    for (int y = 4; y < viewer.viewport()->height() && padlock.x() < 0; y += 12) {
        for (int x = 4; x < viewer.viewport()->width(); x += 12) {
            mouse(viewer, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::NoButton);
            if (viewer.viewport()->cursor().shape() == Qt::PointingHandCursor) {
                padlock = QPoint(x, y);
                break;
            }
        }
    }
    if (!check(padlock.x() >= 0, "The padlocks must expose a clickable hit area"))
        return false;
    mouse(viewer, QEvent::MouseButtonPress, padlock, Qt::LeftButton, Qt::LeftButton);
    mouse(viewer, QEvent::MouseButtonRelease, padlock, Qt::LeftButton, Qt::NoButton);
    int lockedCount = 0;
    for (int vertex = 0; vertex < 3; ++vertex)
        lockedCount += viewer.measurementVertexLocked(vertex);
    if (!check(lockedCount == 1 && vertices(viewer) == points,
               "Clicking a padlock must toggle one vertex without moving it"))
        return false;

    unlock(viewer);
    viewer.setMeasurementVertexLocked(0, true);
    const auto before = vertices(viewer);
    drag(viewer, before[0], before[0] + QPointF(20, 10));
    doubleClick(viewer, viewer.mapFromScene(before[0]));
    if (!check(vertices(viewer) == before && (!editor(viewer) || !editor(viewer)->isVisible()),
               "A locked vertex must reject dragging and the coordinate editor"))
        return false;

    bool openedLength = false;
    for (int y = 4; y < viewer.viewport()->height() && !openedLength; y += 12) {
        for (int x = 4; x < viewer.viewport()->width() && !openedLength; x += 12) {
            const QPoint point(x, y);
            mouse(viewer, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
            if (viewer.viewport()->cursor().shape() != Qt::IBeamCursor)
                continue;
            doubleClick(viewer, point);
            QWidget *panel = editor(viewer);
            if (!panel || !panel->isVisible())
                continue;
            auto *length = panel->findChild<QDoubleSpinBox*>("measurementSideLength");
            openedLength = length && length->isVisible() && panel->accessibleName().startsWith("AB");
            if (openedLength) {
                const double target = viewer.measurementSideLength(0) + 25;
                length->setValue(target);
                QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(length, &enter);
                if (!check(close(viewer.measurementSideLength(0), target)
                           && close(viewer.measurementVertex(0), before[0])
                           && close(viewer.measurementVertex(2), before[2]),
                           "The on-viewer length editor must honor the locks"))
                    return false;
            } else {
                QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(panel, &escape);
            }
        }
    }
    if (!check(openedLength, "Eligible side labels must open their numeric length editor"))
        return false;
    unlock(viewer);
    return true;
}

bool circleChecks(ImageViewer &viewer)
{
    unlock(viewer);
    const auto saved = vertices(viewer);
    const QPointF base(1e9 + 0.125, -1e9 + 0.375);
    viewer.setMeasurementVertex(0, base);
    viewer.setMeasurementVertex(1, base + QPointF(4000, 0));
    viewer.setMeasurementVertex(2, base + QPointF(0, 3000));
    const MeasurementCircle circle = viewer.measurementCircumcircle();
    if (!check(circle.valid && !circle.unstable && close(circle.center, base + QPointF(2000, 1500))
               && close(circle.radius, 2500), "Circumcircle must remain accurate at large coordinate offsets"))
        return false;
    for (const QPointF &point : vertices(viewer)) {
        if (!check(close(QLineF(circle.center, point).length(), circle.radius),
                   "All three vertices must lie on the reported circle"))
            return false;
    }
    viewer.setMeasurementVertex(0, QPointF(0, 0));
    viewer.setMeasurementVertex(1, QPointF(1000, 0));
    viewer.setMeasurementVertex(2, QPointF(500, 1e-5));
    const MeasurementCircle flat = viewer.measurementCircumcircle();
    if (!check(flat.valid && flat.unstable, "Nearly collinear points must flag an unstable center"))
        return false;
    viewer.setMeasurementCircleVisible(true);
    if (!check(!viewer.viewport()->grab().isNull(), "Huge circumcircles must render without unbounded work"))
        return false;
    viewer.setMeasurementVertex(2, QPointF(500, 0));
    if (!check(!viewer.measurementCircumcircle().valid, "Collinear points must not report a circle"))
        return false;
    viewer.setMeasurementVertex(2, QPointF(0, 0));
    if (!check(!viewer.measurementCircumcircle().valid, "Coincident points must not report a circle"))
        return false;
    for (int vertex = 0; vertex < 3; ++vertex)
        viewer.setMeasurementVertex(vertex, saved[vertex]);
    const auto before = vertices(viewer);
    viewer.setMeasurementCircleVisible(false);
    if (!check(!viewer.measurementCircleVisible() && vertices(viewer) == before,
               "Hiding the circumcircle must preserve the triangle"))
        return false;
    viewer.setMeasurementCircleVisible(true);
    return true;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    QSplitter split;
    auto *left = new ImageViewer(&split);
    auto *right = new ImageViewer(&split);
    split.resize(1200, 700);
    left->layerManager()->addLayer(new CanvasLayer);
    right->layerManager()->addLayer(new CanvasLayer);
    split.show();
    app.processEvents();
    left->setMeasureDistanceEnabled(true);
    right->setMeasureDistanceEnabled(true);

    if (!panRepaintChecks(*left, *right))
        return 28;

    if (!check(left->measurementVisible()
               && close(left->measurementAngleDegrees(0), 90)
               && close(left->measurementAngleDegrees(1), 45)
               && close(left->measurementAngleDegrees(2), 45)
               && close(left->measurementSideLength(1),
                        std::hypot(left->measurementSideLength(0), left->measurementSideLength(2))),
               "Activation must show a closed right triangle with three lengths and angles"))
        return 1;
    for (const QPointF &point : vertices(*left)) {
        if (!check(left->viewport()->rect().contains(left->mapFromScene(point)),
                   "Initial vertices must be inside the viewport"))
            return 2;
    }

    const auto rightBefore = vertices(*right);
    for (int vertex = 0; vertex < 3; ++vertex) {
        const auto before = vertices(*left);
        const QPointF target = before[vertex] + QPointF(20.125, -10.375);
        left->setMeasurementVertex(vertex, target);
        for (int other = 0; other < 3; ++other) {
            if (!check(close(left->measurementVertex(other), other == vertex ? target : before[other]),
                       "Numeric editing must move only the chosen vertex"))
                return 3;
        }
    }
    if (!check(vertices(*right) == rightBefore, "Measurement edits must remain local to the viewer"))
        return 4;

    // Put all handles at integer scene positions for exact drag comparisons.
    const QPointF center = left->mapToScene(left->viewport()->rect().center());
    left->setMeasurementVertex(0, center + QPointF(-100, 80));
    left->setMeasurementVertex(1, center + QPointF(100, 80));
    left->setMeasurementVertex(2, center + QPointF(-100, -100));
    for (int vertex = 0; vertex < 3; ++vertex) {
        const auto before = vertices(*left);
        const QPointF target = before[vertex] + QPointF(20, 10);
        drag(*left, before[vertex], target);
        for (int other = 0; other < 3; ++other) {
            if (!check(close(left->measurementVertex(other), other == vertex ? target : before[other]),
                       "Every handle must move independently"))
                return 5;
        }
    }

    const auto beforeTranslation = vertices(*left);
    const QPointF centroid = (beforeTranslation[0] + beforeTranslation[1] + beforeTranslation[2]) / 3;
    const double angleBefore = left->measurementAngleDegrees(0);
    drag(*left, centroid, centroid + QPointF(30, -20));
    for (int vertex = 0; vertex < 3; ++vertex) {
        if (!check(close(left->measurementVertex(vertex), beforeTranslation[vertex] + QPointF(30, -20)),
                   "Dragging inside must translate the whole triangle"))
            return 6;
    }
    if (!check(close(left->measurementAngleDegrees(0), angleBefore),
               "Whole-triangle translation must preserve the angles"))
        return 7;

    for (int vertex = 0; vertex < 3; ++vertex) {
        const auto before = vertices(*left);
        doubleClick(*left, left->mapFromScene(before[vertex]));
        if (!check(editor(*left) && editor(*left)->isVisible(),
                   "Double-clicking a vertex must open its on-viewer coordinate editor"))
            return 8;
        auto *x = editor(*left)->findChild<QDoubleSpinBox*>("measurementVertexX");
        auto *y = editor(*left)->findChild<QDoubleSpinBox*>("measurementVertexYDown");
        if (!check(x && y && close(x->value(), before[vertex].x())
                   && close(y->value(), before[vertex].y()), "The editor must show that vertex's coordinates"))
            return 9;
        const QPointF target = before[vertex] + QPointF(0.5, -0.25);
        x->setValue(target.x());
        y->setValue(target.y());
        if (!check(vertices(*left) == before, "Draft coordinates must not apply before confirmation"))
            return 10;
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(x, &enter);
        for (int other = 0; other < 3; ++other) {
            if (!check(close(left->measurementVertex(other), other == vertex ? target : before[other]),
                       "Enter must apply only the edited vertex"))
                return 11;
        }
        if (!check(!editor(*left)->isVisible(), "Confirmation must close the editor"))
            return 12;
    }

    const auto beforeCancel = vertices(*left);
    doubleClick(*left, left->mapFromScene(beforeCancel[1]));
    auto *x = editor(*left)->findChild<QDoubleSpinBox*>("measurementVertexX");
    x->setValue(x->value() + 40);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(x, &escape);
    if (!check(vertices(*left) == beforeCancel && left->measurementVisible()
               && !editor(*left)->isVisible(), "Escape in the editor must cancel without clearing the triangle"))
        return 13;

    QPoint coordinateLabel(-1, -1);
    const QPoint aScreen = left->mapFromScene(left->measurementVertex(0));
    for (int y = qMax(4, aScreen.y() - 80);
         y < qMin(left->viewport()->height(), aScreen.y() + 100) && coordinateLabel.x() < 0; y += 12) {
        for (int xPos = qMax(4, aScreen.x() - 260);
             xPos < qMin(left->viewport()->width(), aScreen.x() + 200); xPos += 12) {
            mouse(*left, QEvent::MouseMove, QPoint(xPos, y), Qt::NoButton, Qt::NoButton);
            if (left->viewport()->cursor().shape() == Qt::IBeamCursor) {
                coordinateLabel = QPoint(xPos, y);
                break;
            }
        }
    }
    if (!check(coordinateLabel.x() >= 0, "Coordinate labels must advertise editing with the cursor"))
        return 14;
    doubleClick(*left, coordinateLabel);
    if (!check(editor(*left)->isVisible(), "Double-clicking the coordinate text must open its editor"))
        return 15;
    left->setZoomPercent(200);
    if (!check(!editor(*left)->isVisible(), "Zoom must dismiss an editor tied to an old screen position"))
        return 16;

    const QPointF beforeZoomDrag = left->measurementVertex(1);
    const QPoint screen = left->mapFromScene(beforeZoomDrag);
    mouse(*left, QEvent::MouseButtonPress, screen + QPoint(6, 0), Qt::LeftButton, Qt::LeftButton);
    mouse(*left, QEvent::MouseMove, screen + QPoint(26, 0), Qt::NoButton, Qt::LeftButton);
    mouse(*left, QEvent::MouseButtonRelease, screen + QPoint(26, 0), Qt::LeftButton, Qt::NoButton);
    if (!check(close(left->measurementVertex(1), beforeZoomDrag + QPointF(10, 0)),
               "Handle capture must remain screen-sized and must not jump on press"))
        return 17;

    const QPoint panHandle = left->mapFromScene(left->measurementVertex(1));
    mouse(*left, QEvent::MouseButtonPress, panHandle, Qt::LeftButton, Qt::LeftButton);
    const auto beforePan = vertices(*left);
    key(*left, QEvent::KeyPress, Qt::Key_Space);
    mouse(*left, QEvent::MouseButtonRelease, panHandle + QPoint(30, 30), Qt::LeftButton, Qt::NoButton);
    key(*right, QEvent::KeyRelease, Qt::Key_Space);
    if (!check(vertices(*left) == beforePan, "Temporary pan across split panels must cancel vertex dragging"))
        return 18;

    const QPointF origin = left->measurementVertex(0);
    left->setMeasurementVertex(1, origin + QPointF(80, 0));
    left->setMeasurementVertex(2, origin + QPointF(-80, 0));
    if (!check(close(left->measurementAngleDegrees(0), 180)
               && close(left->measurementAngleDegrees(1), 0),
               "A collinear triangle must still report its defined angles"))
        return 19;
    left->setMeasurementVertex(1, origin);
    if (!check(!std::isfinite(left->measurementAngleDegrees(0)), "Coincident vertices have undefined angles"))
        return 20;
    drag(*left, origin, origin + QPointF(50, 0));
    if (!check(close(left->measurementVertex(1), origin + QPointF(50, 0))
               && close(left->measurementVertex(0), origin),
               "Coincident vertices must remain separable by dragging"))
        return 21;

    if (!lockChecks(*left) || !circleChecks(*left))
        return 27;

    const QImage rendered = right->viewport()->grab().toImage();
    if (!check(!rendered.isNull(), "Triangle annotations must render"))
        return 22;
    if (argc > 1) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        rendered.save(path);
        split.resize(1800, 950);
        app.processEvents();
        right->viewport()->grab().save(path + ".wide.png");
        doubleClick(*right, right->mapFromScene(right->measurementVertex(2)));
        right->viewport()->grab().save(path + ".editor.png");
        QKeyEvent cancelEditor(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(editor(*right)->findChild<QDoubleSpinBox*>("measurementVertexX"),
                                &cancelEditor);
        split.resize(700, 600);
        app.processEvents();
        right->setMeasureDistanceEnabled(false);
        right->setMeasureDistanceEnabled(true);
        right->viewport()->grab().save(path + ".narrow.png");
    }

    doubleClick(*left, left->mapFromScene(left->measurementVertex(2)));
    left->hide();
    if (!check(!editor(*left)->isVisible(), "Hiding a viewer must dismiss its coordinate editor"))
        return 23;
    left->show();
    app.processEvents();
    key(*left, QEvent::KeyPress, Qt::Key_Escape);
    if (!check(!left->measurementVisible() && right->measurementVisible(), "Escape must clear only this viewer"))
        return 24;
    mouse(*left, QEvent::MouseButtonPress, left->viewport()->rect().center(), Qt::LeftButton, Qt::LeftButton);
    mouse(*left, QEvent::MouseButtonRelease, left->viewport()->rect().center(), Qt::LeftButton, Qt::NoButton);
    if (!check(left->measurementVisible() && close(left->measurementAngleDegrees(0), 90),
               "A click after clearing must restore the triangle"))
        return 25;
    left->layerManager()->clearAll();
    if (!check(!left->measureDistanceEnabled() && !left->measurementVisible(),
               "Removing the last layer must disable measurement"))
        return 26;
    qInfo() << "Triangle measurement checks passed";
    return 0;
}
