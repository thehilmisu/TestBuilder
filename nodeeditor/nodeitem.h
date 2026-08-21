#ifndef NODEEDITOR_NODEITEM_H
#define NODEEDITOR_NODEITEM_H

#include "blocktypes.h"

#include <QFont>
#include <QGraphicsItem>
#include <QVariantMap>
#include <QVector>
#include <QAtomicInteger>

namespace nodeeditor {

class PortItem;

// One block on the canvas: a titled box with input ports on the left and
// output ports on the right, plus a parameter set taken from its BlockType.
class NodeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 3 };

    // Where the runner currently is. Drawn as a coloured halo so a run can be
    // followed on the canvas; purely cosmetic and never serialized.
    enum class RunState { Idle, Active, Visited, Failed };

    explicit NodeItem(const BlockType *blockType, QGraphicsItem *parent = nullptr);
    // Copy constructor – generates a NEW ID (the copy is a new instantiation)
    NodeItem(const NodeItem& other)
        : m_id(nextId.fetchAndAddAcquire(1)) {
        // Copy other member data here
    }

    // Move constructor – also generates a NEW ID
    NodeItem(NodeItem&& other) noexcept
        : m_id(nextId.fetchAndAddAcquire(1)) {
        // Steal resources, keep new ID
    }

    // Copy-assignment – ID stays unchanged (object already exists)
    NodeItem& operator=(const NodeItem& other) {
        if (this != &other) {
            // Copy other data, but DO NOT modify m_id
        }
        return *this;
    }

    // Move-assignment – ID stays unchanged
    NodeItem& operator=(NodeItem&& other) noexcept {
        if (this != &other) {
            // Steal other's resources, keep this->m_id
        }
        return *this;
    }

    quint64 getId() const { return m_id; }

    // Only for the loader, which has to restore the ids the links refer to.
    // Pushes the shared counter past `id` so later blocks never collide with it.
    void setId(quint64 id);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const BlockType *blockType() const { return m_type; }
    QString typeId() const { return m_type->id; }
    QColor accent() const { return m_type->accent; }

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    QVariant param(const QString &key) const { return m_params.value(key); }
    const QVariantMap &params() const { return m_params; }
    void setParam(const QString &key, const QVariant &value);

    RunState runState() const { return m_runState; }
    void setRunState(RunState state);

    const QVector<PortItem *> &inputs() const { return m_inputs; }
    const QVector<PortItem *> &outputs() const { return m_outputs; }
    QVector<PortItem *> allPorts() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    static QAtomicInteger<quint64> nextId;  // quint64 prevents overflow
    quint64 m_id;

    void buildPorts();
    void relayout();
    QStringList summaryLines() const;

    const BlockType *m_type;
    RunState m_runState = RunState::Idle;
    QString m_title;
    QVariantMap m_params;
    QVector<PortItem *> m_inputs;
    QVector<PortItem *> m_outputs;

    QFont m_titleFont;
    QFont m_bodyFont;
    qreal m_width = 180.0;
    qreal m_height = 80.0;
};

} // namespace nodeeditor

#endif // NODEEDITOR_NODEITEM_H
