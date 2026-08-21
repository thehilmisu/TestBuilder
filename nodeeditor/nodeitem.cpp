#include "nodeitem.h"

#include "connectionitem.h"
#include "portitem.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace nodeeditor {

namespace {
constexpr qreal kHeaderHeight = 26.0;
constexpr qreal kRowHeight = 22.0;
constexpr qreal kTopPadding = 8.0;
constexpr qreal kBottomPadding = 8.0;
constexpr qreal kSidePadding = 12.0;
constexpr qreal kSummaryLineHeight = 15.0;
constexpr qreal kMinWidth = 170.0;
constexpr qreal kMaxWidth = 280.0;
constexpr qreal kCornerRadius = 8.0;
constexpr int kMaxSummaryLines = 3;

const QColor kBodyColor(0x2b, 0x30, 0x3b);
const QColor kBorderColor(0x16, 0x19, 0x1f);
const QColor kTextColor(0xe6, 0xea, 0xf2);
const QColor kMutedColor(0x9b, 0xa5, 0xb6);
const QColor kSelectionColor(0xf2, 0xa8, 0x54);
}

QAtomicInteger<quint64> NodeItem::nextId{1};

void NodeItem::setId(quint64 id)
{
    m_id = id;
    // Never hand out an id at or below one already in use: a block dropped
    // after a load would otherwise take the id of a block from the file.
    quint64 expected = nextId.loadAcquire();
    while (expected <= id && !nextId.testAndSetOrdered(expected, id + 1))
        expected = nextId.loadAcquire();
}

NodeItem::NodeItem(const BlockType *blockType, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_id(nextId.fetchAndAddAcquire(1))
    , m_type(blockType)
    , m_title(blockType->title)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);
    setToolTip(blockType->description);

    m_titleFont.setBold(true);
    m_titleFont.setPointSizeF(m_titleFont.pointSizeF() + 0.5);
    m_bodyFont.setPointSizeF(qMax(7.0, m_bodyFont.pointSizeF() - 1.0));

    for (const ParamSpec &spec : m_type->params)
        m_params.insert(spec.key, spec.defaultValue);

    buildPorts();
    relayout();
}

void NodeItem::buildPorts()
{
    for (int i = 0; i < m_type->inputs.size(); ++i)
        m_inputs.append(new PortItem(PortItem::Input, m_type->inputs.at(i).name, i, this));
    for (int i = 0; i < m_type->outputs.size(); ++i)
        m_outputs.append(new PortItem(PortItem::Output, m_type->outputs.at(i).name, i, this));
}

QVector<PortItem *> NodeItem::allPorts() const
{
    QVector<PortItem *> ports = m_inputs;
    ports += m_outputs;
    return ports;
}

void NodeItem::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    relayout();
}

void NodeItem::setRunState(RunState state)
{
    if (m_runState == state)
        return;
    m_runState = state;
    update();
}

void NodeItem::setParam(const QString &key, const QVariant &value)
{
    if (m_params.value(key) == value)
        return;
    m_params.insert(key, value);
    relayout();
}

QStringList NodeItem::summaryLines() const
{
    QStringList lines;
    for (const ParamSpec &spec : m_type->params) {
        if (lines.size() >= kMaxSummaryLines)
            break;
        const QVariant value = m_params.value(spec.key);
        if (spec.type == ParamSpec::Boolean) {
            if (value.toBool())
                lines << spec.label;
            continue;
        }
        const QString text = value.toString();
        if (text.isEmpty())
            continue;
        lines << QStringLiteral("%1: %2").arg(spec.label, text);
    }
    return lines;
}

void NodeItem::relayout()
{
    const QFontMetricsF titleMetrics(m_titleFont);
    const QFontMetricsF bodyMetrics(m_bodyFont);

    // Width: whatever the widest piece of text needs, clamped to a sane range.
    qreal width = titleMetrics.horizontalAdvance(m_title) + 2 * kSidePadding + 8.0;

    const int rows = qMax(m_inputs.size(), m_outputs.size());
    for (int i = 0; i < rows; ++i) {
        qreal rowWidth = 2 * kSidePadding + 16.0;
        if (i < m_inputs.size())
            rowWidth += bodyMetrics.horizontalAdvance(m_inputs.at(i)->name());
        if (i < m_outputs.size())
            rowWidth += bodyMetrics.horizontalAdvance(m_outputs.at(i)->name());
        width = qMax(width, rowWidth);
    }

    const QStringList summary = summaryLines();
    for (const QString &line : summary)
        width = qMax(width, bodyMetrics.horizontalAdvance(line) + 2 * kSidePadding);

    width = qBound(kMinWidth, width, kMaxWidth);

    qreal height = kHeaderHeight + kTopPadding + rows * kRowHeight + kBottomPadding;
    if (!summary.isEmpty())
        height += summary.size() * kSummaryLineHeight + 4.0;

    prepareGeometryChange();
    m_width = width;
    m_height = height;

    const qreal firstRowY = kHeaderHeight + kTopPadding + kRowHeight / 2.0;
    for (int i = 0; i < m_inputs.size(); ++i)
        m_inputs.at(i)->setPos(0.0, firstRowY + i * kRowHeight);
    for (int i = 0; i < m_outputs.size(); ++i)
        m_outputs.at(i)->setPos(m_width, firstRowY + i * kRowHeight);

    for (PortItem *port : allPorts()) {
        for (ConnectionItem *connection : port->connections())
            connection->updatePath();
    }
    update();
}

QRectF NodeItem::boundingRect() const
{
    // -6 rather than -2: the run halo is drawn 3px outside the body with a 3px
    // pen, and anything outside boundingRect() leaves artefacts when it moves.
    return QRectF(0, 0, m_width, m_height).adjusted(-6, -6, 6, 6);
}

QPainterPath NodeItem::shape() const
{
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, m_width, m_height), kCornerRadius, kCornerRadius);
    return path;
}

void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect(0, 0, m_width, m_height);

    QPainterPath body;
    body.addRoundedRect(rect, kCornerRadius, kCornerRadius);
    painter->fillPath(body, kBodyColor);

    // Header: rounded on top, square where it meets the body.
    QPainterPath header;
    header.addRoundedRect(QRectF(0, 0, m_width, kHeaderHeight * 1.5), kCornerRadius, kCornerRadius);
    header = header.intersected(body);
    QPainterPath headerClip;
    headerClip.addRect(QRectF(0, 0, m_width, kHeaderHeight));
    painter->fillPath(header.intersected(headerClip), m_type->accent);

    // The run halo sits outside the border so it reads even on a selected node.
    if (m_runState != RunState::Idle) {
        static const QColor kActiveColor(0x4f, 0xc3, 0xf7);
        static const QColor kVisitedColor(0x3d, 0x6b, 0x55);
        static const QColor kFailedColor(0xd6, 0x4b, 0x4b);

        QColor halo = kVisitedColor;
        qreal haloWidth = 2.0;
        if (m_runState == RunState::Active) {
            halo = kActiveColor;
            haloWidth = 3.0;
        } else if (m_runState == RunState::Failed) {
            halo = kFailedColor;
            haloWidth = 3.0;
        }
        painter->setPen(QPen(halo, haloWidth));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(-3, -3, 3, 3), kCornerRadius + 3, kCornerRadius + 3);
    }

    painter->setPen(QPen(isSelected() ? kSelectionColor : kBorderColor, isSelected() ? 2.0 : 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(body);

    painter->setFont(m_titleFont);
    painter->setPen(kTextColor);
    const QRectF titleRect(kSidePadding, 0, m_width - 2 * kSidePadding, kHeaderHeight);
    painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetricsF(m_titleFont).elidedText(m_title, Qt::ElideRight, titleRect.width()));

    painter->setFont(m_bodyFont);

    const qreal firstRowY = kHeaderHeight + kTopPadding;
    const int rows = qMax(m_inputs.size(), m_outputs.size());
    for (int i = 0; i < rows; ++i) {
        const QRectF rowRect(kSidePadding, firstRowY + i * kRowHeight,
                             m_width - 2 * kSidePadding, kRowHeight);
        painter->setPen(kTextColor);
        if (i < m_inputs.size())
            painter->drawText(rowRect, Qt::AlignVCenter | Qt::AlignLeft, m_inputs.at(i)->name());
        if (i < m_outputs.size())
            painter->drawText(rowRect, Qt::AlignVCenter | Qt::AlignRight, m_outputs.at(i)->name());
    }

    const QStringList summary = summaryLines();
    if (summary.isEmpty())
        return;

    qreal y = firstRowY + rows * kRowHeight + 2.0;
    painter->setPen(QPen(QColor(0x3a, 0x41, 0x50), 1.0));
    painter->drawLine(QPointF(kSidePadding, y), QPointF(m_width - kSidePadding, y));
    y += 2.0;

    painter->setPen(kMutedColor);
    const QFontMetricsF bodyMetrics(m_bodyFont);
    for (const QString &line : summary) {
        const QRectF lineRect(kSidePadding, y, m_width - 2 * kSidePadding, kSummaryLineHeight);
        painter->drawText(lineRect, Qt::AlignVCenter | Qt::AlignLeft,
                          bodyMetrics.elidedText(line, Qt::ElideRight, lineRect.width()));
        y += kSummaryLineHeight;
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        for (PortItem *port : allPorts()) {
            for (ConnectionItem *connection : port->connections())
                connection->updatePath();
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

} // namespace nodeeditor
