#include "portitem.h"

#include "connectionitem.h"
#include "nodeitem.h"

#include <QGraphicsSceneHoverEvent>
#include <QPainter>

namespace nodeeditor {

namespace {
constexpr qreal kRadius = 5.0;
constexpr qreal kHitMargin = 5.0;
}

PortItem::PortItem(Direction direction, const QString &name, int index, NodeItem *node)
    : QGraphicsItem(node)
    , m_direction(direction)
    , m_name(name)
    , m_index(index)
    , m_node(node)
{
    setAcceptHoverEvents(true);
    setZValue(1.0);
    setToolTip(name);
}

QRectF PortItem::boundingRect() const
{
    const qreal r = kRadius + kHitMargin;
    return QRectF(-r, -r, 2 * r, 2 * r);
}

void PortItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const bool active = m_hovered || m_highlighted;
    const qreal r = active ? kRadius + 1.5 : kRadius;

    QColor fill = isConnected() || active ? m_node->accent().lighter(active ? 150 : 115)
                                          : QColor(0x9a, 0xa4, 0xb4);
    if (m_highlighted)
        fill = QColor(0x6f, 0xd0, 0x8c);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(0x15, 0x18, 0x1e), 1.5));
    painter->setBrush(fill);
    painter->drawEllipse(QPointF(0, 0), r, r);
}

void PortItem::addConnection(ConnectionItem *connection)
{
    if (connection && !m_connections.contains(connection))
        m_connections.append(connection);
    update();
}

void PortItem::removeConnection(ConnectionItem *connection)
{
    m_connections.removeAll(connection);
    update();
}

bool PortItem::canConnectTo(const PortItem *other) const
{
    if (!other || other == this)
        return false;
    if (other->node() == m_node)
        return false;
    if (other->direction() == m_direction)
        return false;

    // Reject an edge that already exists between exactly these two ports.
    for (const ConnectionItem *c : m_connections) {
        if (c->source() == other || c->dest() == other)
            return false;
    }
    return true;
}

void PortItem::setHighlighted(bool on)
{
    if (m_highlighted == on)
        return;
    m_highlighted = on;
    update();
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

} // namespace nodeeditor
