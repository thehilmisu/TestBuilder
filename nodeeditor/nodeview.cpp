#include "nodeview.h"

#include "compat.h"
#include "nodescene.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace nodeeditor {

namespace {
constexpr qreal kMinScale = 0.2;
constexpr qreal kMaxScale = 3.0;
constexpr qreal kZoomStep = 1.15;
}

NodeView::NodeView(NodeScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                   QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(true); // propagates to the viewport, which forwards to the scene
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void NodeView::applyZoom(qreal factor)
{
    const qreal current = transform().m11();
    const qreal target = qBound(kMinScale, current * factor, kMaxScale);
    if (qFuzzyCompare(current, target))
        return;
    scale(target / current, target / current);
}

void NodeView::zoomIn()
{
    applyZoom(kZoomStep);
}

void NodeView::zoomOut()
{
    applyZoom(1.0 / kZoomStep);
}

void NodeView::resetZoom()
{
    resetTransform();
}

void NodeView::fitGraphInView()
{
    const QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isEmpty()) {
        resetTransform();
        centerOn(0, 0);
        return;
    }
    fitInView(bounds.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    if (transform().m11() > 1.0)
        resetTransform();
}

void NodeView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    applyZoom(delta > 0 ? kZoomStep : 1.0 / kZoomStep);
    event->accept();
}

void NodeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPos = eventPos(event);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void NodeView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint pos = eventPos(event);
        const QPoint delta = pos - m_lastPanPos;
        m_lastPanPos = pos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void NodeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && event->button() == Qt::MiddleButton) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace nodeeditor
