#ifndef NODEEDITOR_NODEITEM_H
#define NODEEDITOR_NODEITEM_H

#include "blocktypes.h"

#include <QFont>
#include <QGraphicsItem>
#include <QVariantMap>
#include <QVector>

namespace nodeeditor {

class PortItem;

// One block on the canvas: a titled box with input ports on the left and
// output ports on the right, plus a parameter set taken from its BlockType.
class NodeItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 3 };

    explicit NodeItem(const BlockType *blockType, QGraphicsItem *parent = nullptr);

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

    const QVector<PortItem *> &inputs() const { return m_inputs; }
    const QVector<PortItem *> &outputs() const { return m_outputs; }
    QVector<PortItem *> allPorts() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void buildPorts();
    void relayout();
    QStringList summaryLines() const;

    const BlockType *m_type;
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
