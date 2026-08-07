#include "connectionitem.h"

#include "nodeitem.h"
#include "portitem.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QtMath>

namespace nodeeditor {

ConnectionItem::ConnectionItem()
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setZValue(-1.0);
    setAcceptedMouseButtons(Qt::LeftButton);
}

ConnectionItem::~ConnectionItem()
{
    if (m_source)
        m_source->removeConnection(this);
    if (m_dest)
        m_dest->removeConnection(this);
}

void ConnectionItem::setSource(PortItem *port)
{
    if (m_source == port)
        return;
    if (m_source)
        m_source->removeConnection(this);
    m_source = port;
    if (m_source)
        m_source->addConnection(this);
    updatePath();
}

void ConnectionItem::setDest(PortItem *port)
{
    if (m_dest == port)
        return;
    if (m_dest)
        m_dest->removeConnection(this);
    m_dest = port;
    if (m_dest)
        m_dest->addConnection(this);
    updatePath();
}

void ConnectionItem::setDragPoint(const QPointF &scenePos)
{
    m_dragPoint = scenePos;
    updatePath();
}

void ConnectionItem::detach(PortItem *port)
{
    if (m_source == port) {
        m_source->removeConnection(this);
        m_source = nullptr;
    }
    if (m_dest == port) {
        m_dest->removeConnection(this);
        m_dest = nullptr;
    }
}

QPointF ConnectionItem::sourcePoint() const
{
    return m_source ? m_source->anchorScenePos() : m_dragPoint;
}

QPointF ConnectionItem::destPoint() const
{
    return m_dest ? m_dest->anchorScenePos() : m_dragPoint;
}

void ConnectionItem::updatePath()
{
    const QPointF p1 = sourcePoint();
    const QPointF p2 = destPoint();

    // Horizontal tangents give the familiar "cable" look and keep the curve
    // readable even when the target sits to the left of the source.
    const qreal dx = qMax(qAbs(p2.x() - p1.x()) * 0.5, 60.0);

    QPainterPath path(p1);
    path.cubicTo(p1 + QPointF(dx, 0), p2 - QPointF(dx, 0), p2);

    prepareGeometryChange();
    setPath(path);
}

QPainterPath ConnectionItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(12.0);
    return stroker.createStroke(path());
}

void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QColor color(0x8a, 0x93, 0xa5);
    if (m_source && m_source->node())
        color = m_source->node()->accent().lighter(130);
    else if (m_dest && m_dest->node())
        color = m_dest->node()->accent().lighter(130);

    qreal width = 2.0;
    Qt::PenStyle style = Qt::SolidLine;
    if (!isComplete()) {
        style = Qt::DashLine; // link still being dragged
        color = QColor(0xd0, 0xd6, 0xe2);
    }
    if (isSelected()) {
        color = QColor(0xf2, 0xa8, 0x54);
        width = 3.0;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(color, width, style, Qt::RoundCap, Qt::RoundJoin));
    painter->drawPath(path());

    if (!isComplete())
        return;

    // Direction arrow at the midpoint of the curve.
    const qreal t = 0.5;
    const QPointF mid = path().pointAtPercent(t);
    const qreal angle = -qDegreesToRadians(path().angleAtPercent(t));
    const QPointF dir(qCos(angle), qSin(angle));
    const QPointF normal(-dir.y(), dir.x());

    QPolygonF head;
    head << mid + dir * 6.0 << mid - dir * 4.0 + normal * 4.0 << mid - dir * 4.0 - normal * 4.0;

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(head);
}

} // namespace nodeeditor
