#ifndef NODEEDITOR_PORTITEM_H
#define NODEEDITOR_PORTITEM_H

#include <QGraphicsItem>
#include <QString>
#include <QVector>

namespace nodeeditor {

class ConnectionItem;
class NodeItem;

// A single connection point on a node. Ports are children of their node, so
// they follow it automatically when it is dragged.
class PortItem : public QGraphicsItem
{
public:
    enum Direction { Input, Output };
    enum { Type = UserType + 1 };

    PortItem(Direction direction, const QString &name, int index, NodeItem *node);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    Direction direction() const { return m_direction; }
    bool isInput() const { return m_direction == Input; }
    bool isOutput() const { return m_direction == Output; }
    const QString &name() const { return m_name; }
    int index() const { return m_index; }
    NodeItem *node() const { return m_node; }

    QPointF anchorScenePos() const { return mapToScene(QPointF(0, 0)); }

    void addConnection(ConnectionItem *connection);
    void removeConnection(ConnectionItem *connection);
    const QVector<ConnectionItem *> &connections() const { return m_connections; }
    bool isConnected() const { return !m_connections.isEmpty(); }

    // True when a link from this port to \a other would be a legal new edge.
    bool canConnectTo(const PortItem *other) const;

    void setHighlighted(bool on);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    Direction m_direction;
    QString m_name;
    int m_index;
    NodeItem *m_node;
    QVector<ConnectionItem *> m_connections;
    bool m_hovered = false;
    bool m_highlighted = false;
};

} // namespace nodeeditor

#endif // NODEEDITOR_PORTITEM_H
