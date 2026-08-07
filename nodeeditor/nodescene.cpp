#include "nodescene.h"

#include "blocktypes.h"
#include "connectionitem.h"
#include "nodeitem.h"
#include "portitem.h"

#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>

#include <cmath>

namespace nodeeditor {

namespace {
const QColor kCanvasColor(0x1e, 0x22, 0x2a);
const QColor kGridFine(0x27, 0x2c, 0x36);
const QColor kGridCoarse(0x31, 0x38, 0x45);
constexpr qreal kGridStep = 20.0;
constexpr int kCoarseEvery = 5;
}

NodeScene::NodeScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-5000, -5000, 10000, 10000);
    connect(this, &QGraphicsScene::selectionChanged, this, &NodeScene::emitSelection);
}

NodeItem *NodeScene::addNode(const BlockType *type, const QPointF &scenePos)
{
    if (!type)
        return nullptr;

    auto *node = new NodeItem(type);
    node->setPos(scenePos);
    addItem(node);
    emit graphChanged();
    return node;
}

NodeItem *NodeScene::addNode(const QString &typeId, const QPointF &scenePos)
{
    return addNode(BlockLibrary::find(typeId), scenePos);
}

ConnectionItem *NodeScene::connectPorts(PortItem *a, PortItem *b)
{
    if (!a || !b || !a->canConnectTo(b))
        return nullptr;

    PortItem *from = a->isOutput() ? a : b;
    PortItem *to = a->isOutput() ? b : a;

    // An input takes a single incoming link; a new one replaces the old.
    const QVector<ConnectionItem *> existing = to->connections();
    for (ConnectionItem *c : existing)
        removeConnection(c);

    auto *connection = new ConnectionItem;
    addItem(connection);
    connection->setSource(from);
    connection->setDest(to);
    connection->updatePath();

    emit graphChanged();
    return connection;
}

void NodeScene::removeConnection(ConnectionItem *connection)
{
    if (!connection)
        return;
    if (m_pendingConnection == connection)
        m_pendingConnection = nullptr;
    removeItem(connection);
    delete connection;
    emit graphChanged();
}

void NodeScene::removeNode(NodeItem *node)
{
    if (!node)
        return;

    emit nodeAboutToBeRemoved(node);

    // Drop the edges first so no ConnectionItem outlives the ports it points at.
    for (PortItem *port : node->allPorts()) {
        const QVector<ConnectionItem *> conns = port->connections();
        for (ConnectionItem *c : conns)
            removeConnection(c);
    }

    removeItem(node);
    delete node;
    emit graphChanged();
}

void NodeScene::deleteSelection()
{
    const QList<QGraphicsItem *> selection = selectedItems();

    QVector<ConnectionItem *> deadConnections;
    QVector<NodeItem *> deadNodes;
    for (QGraphicsItem *item : selection) {
        if (auto *c = qgraphicsitem_cast<ConnectionItem *>(item))
            deadConnections.append(c);
        else if (auto *n = qgraphicsitem_cast<NodeItem *>(item))
            deadNodes.append(n);
    }

    for (ConnectionItem *c : deadConnections)
        removeConnection(c);
    for (NodeItem *n : deadNodes)
        removeNode(n);
}

void NodeScene::clearGraph()
{
    cancelPendingConnection();

    const QVector<ConnectionItem *> conns = connections();
    for (ConnectionItem *c : conns)
        removeConnection(c);

    const QVector<NodeItem *> ns = nodes();
    for (NodeItem *n : ns)
        removeNode(n);
}

QVector<NodeItem *> NodeScene::nodes() const
{
    QVector<NodeItem *> result;
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *item : all) {
        if (auto *node = qgraphicsitem_cast<NodeItem *>(item))
            result.append(node);
    }
    return result;
}

QVector<ConnectionItem *> NodeScene::connections() const
{
    QVector<ConnectionItem *> result;
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *item : all) {
        if (auto *c = qgraphicsitem_cast<ConnectionItem *>(item))
            result.append(c);
    }
    return result;
}

PortItem *NodeScene::portAt(const QPointF &scenePos) const
{
    const QList<QGraphicsItem *> hits = items(scenePos);
    for (QGraphicsItem *item : hits) {
        if (auto *port = qgraphicsitem_cast<PortItem *>(item))
            return port;
    }
    return nullptr;
}

void NodeScene::clearPortHighlights()
{
    if (m_hoverTargetPort) {
        m_hoverTargetPort->setHighlighted(false);
        m_hoverTargetPort = nullptr;
    }
}

void NodeScene::cancelPendingConnection()
{
    clearPortHighlights();
    if (m_pendingConnection) {
        ConnectionItem *c = m_pendingConnection;
        m_pendingConnection = nullptr;
        removeItem(c);
        delete c;
    }
    m_dragStartPort = nullptr;
}

void NodeScene::emitSelection()
{
    NodeItem *selected = nullptr;
    const QList<QGraphicsItem *> selection = selectedItems();
    for (QGraphicsItem *item : selection) {
        if (auto *node = qgraphicsitem_cast<NodeItem *>(item)) {
            if (selected) { // more than one node -> no single subject to edit
                selected = nullptr;
                break;
            }
            selected = node;
        }
    }
    emit nodeSelected(selected);
}

void NodeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (PortItem *port = portAt(event->scenePos())) {
            if (port->isInput() && port->isConnected()) {
                // Grabbing a wired input unplugs the link so it can be re-routed.
                ConnectionItem *existing = port->connections().first();
                if (PortItem *origin = existing->source()) {
                    m_dragStartPort = origin;
                    m_pendingConnection = existing;
                    m_pendingConnection->setDest(nullptr);
                }
            }
            if (!m_pendingConnection) {
                m_dragStartPort = port;
                m_pendingConnection = new ConnectionItem;
                addItem(m_pendingConnection);
                if (port->isOutput())
                    m_pendingConnection->setSource(port);
                else
                    m_pendingConnection->setDest(port);
            }
            m_pendingConnection->setDragPoint(event->scenePos());
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void NodeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_pendingConnection) {
        m_pendingConnection->setDragPoint(event->scenePos());

        PortItem *candidate = portAt(event->scenePos());
        if (candidate && !m_dragStartPort->canConnectTo(candidate))
            candidate = nullptr;
        if (candidate != m_hoverTargetPort) {
            clearPortHighlights();
            m_hoverTargetPort = candidate;
            if (m_hoverTargetPort)
                m_hoverTargetPort->setHighlighted(true);
        }
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void NodeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_pendingConnection) {
        PortItem *target = portAt(event->scenePos());
        PortItem *start = m_dragStartPort;

        // The pending item is thrown away either way: a successful drop is
        // rebuilt through connectPorts() so the single-input rule applies.
        cancelPendingConnection();

        if (target && start && start->canConnectTo(target))
            connectPorts(start, target);

        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void NodeScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_pendingConnection) {
        cancelPendingConnection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void NodeScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(blockMimeType())) {
        event->setAccepted(true);
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragEnterEvent(event);
}

void NodeScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(blockMimeType())) {
        event->setAccepted(true);
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragMoveEvent(event);
}

void NodeScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (!event->mimeData()->hasFormat(blockMimeType())) {
        QGraphicsScene::dropEvent(event);
        return;
    }

    const QString typeId = QString::fromUtf8(event->mimeData()->data(blockMimeType()));
    if (NodeItem *node = addNode(typeId, event->scenePos())) {
        clearSelection();
        node->setSelected(true);
    }
    event->acceptProposedAction();
}

void NodeScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, kCanvasColor);
    painter->setRenderHint(QPainter::Antialiasing, false);

    const qreal left = std::floor(rect.left() / kGridStep) * kGridStep;
    const qreal top = std::floor(rect.top() / kGridStep) * kGridStep;

    QVector<QLineF> fine;
    QVector<QLineF> coarse;

    for (qreal x = left; x < rect.right(); x += kGridStep) {
        const bool major = qRound(x / kGridStep) % kCoarseEvery == 0;
        (major ? coarse : fine).append(QLineF(x, rect.top(), x, rect.bottom()));
    }
    for (qreal y = top; y < rect.bottom(); y += kGridStep) {
        const bool major = qRound(y / kGridStep) % kCoarseEvery == 0;
        (major ? coarse : fine).append(QLineF(rect.left(), y, rect.right(), y));
    }

    painter->setPen(QPen(kGridFine, 1.0));
    painter->drawLines(fine);
    painter->setPen(QPen(kGridCoarse, 1.0));
    painter->drawLines(coarse);
}

} // namespace nodeeditor
