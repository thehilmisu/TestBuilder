#ifndef NODEEDITOR_NODESCENE_H
#define NODEEDITOR_NODESCENE_H

#include <QGraphicsScene>
#include <QVector>

namespace nodeeditor {

class ConnectionItem;
class NodeItem;
class PortItem;
struct BlockType;

// The canvas. Owns the graph and all direct manipulation on it: dragging new
// links between ports, dropping blocks from the palette, deleting selection.
class NodeScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit NodeScene(QObject *parent = nullptr);

    NodeItem *addNode(const BlockType *type, const QPointF &scenePos);
    NodeItem *addNode(const QString &typeId, const QPointF &scenePos);

    ConnectionItem *connectPorts(PortItem *a, PortItem *b);

    void removeNode(NodeItem *node);
    void removeConnection(ConnectionItem *connection);
    void deleteSelection();
    void clearGraph();

    QVector<NodeItem *> nodes() const;
    QVector<ConnectionItem *> connections() const;

signals:
    void graphChanged();
    void nodeSelected(nodeeditor::NodeItem *node); // nullptr when nothing is selected
    void nodeAboutToBeRemoved(nodeeditor::NodeItem *node);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

private:
    PortItem *portAt(const QPointF &scenePos) const;
    void clearPortHighlights();
    void cancelPendingConnection();
    void emitSelection();

    ConnectionItem *m_pendingConnection = nullptr;
    PortItem *m_dragStartPort = nullptr;
    PortItem *m_hoverTargetPort = nullptr;
};

} // namespace nodeeditor

#endif // NODEEDITOR_NODESCENE_H
