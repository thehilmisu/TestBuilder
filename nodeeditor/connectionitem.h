#ifndef NODEEDITOR_CONNECTIONITEM_H
#define NODEEDITOR_CONNECTIONITEM_H

#include <QGraphicsPathItem>

namespace nodeeditor {

class PortItem;

// A directed edge between an output port and an input port. While the user is
// dragging a new link, one end is a free scene position instead of a port.
class ConnectionItem : public QGraphicsPathItem
{
public:
    enum { Type = UserType + 2 };

    ConnectionItem();
    ~ConnectionItem() override;

    int type() const override { return Type; }

    PortItem *source() const { return m_source; }
    PortItem *dest() const { return m_dest; }

    void setSource(PortItem *port);
    void setDest(PortItem *port);

    // Free end position used while the link is being dragged.
    void setDragPoint(const QPointF &scenePos);

    bool isComplete() const { return m_source && m_dest; }

    // Detaches this edge from \a port (used when the port's node goes away).
    void detach(PortItem *port);

    void updatePath();

    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPointF sourcePoint() const;
    QPointF destPoint() const;

    PortItem *m_source = nullptr;
    PortItem *m_dest = nullptr;
    QPointF m_dragPoint;
};

} // namespace nodeeditor

#endif // NODEEDITOR_CONNECTIONITEM_H
