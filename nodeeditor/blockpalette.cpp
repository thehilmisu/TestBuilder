#include "blockpalette.h"

#include "blocktypes.h"
#include "compat.h"

#include <QApplication>
#include <QDrag>
#include <QFontMetrics>
#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

namespace nodeeditor {

namespace {
constexpr int kTypeIdRole = Qt::UserRole + 1;
}

BlockPalette::BlockPalette(QWidget *parent)
    : QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setUniformItemSizes(false);
    setAlternatingRowColors(false);
    setIconSize(QSize(14, 14));
    populate();

    connect(this, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const QString id = item->data(kTypeIdRole).toString();
        if (!id.isEmpty())
            emit blockActivated(id);
    });
}

void BlockPalette::populate()
{
    for (const QString &category : BlockLibrary::categories()) {
        auto *header = new QListWidgetItem(category.toUpper(), this);
        header->setFlags(Qt::NoItemFlags);
        QFont headerFont = header->font();
        headerFont.setBold(true);
        headerFont.setPointSizeF(qMax(7.0, headerFont.pointSizeF() - 1.0));
        header->setFont(headerFont);
        header->setForeground(QColor(0x80, 0x88, 0x96));

        for (const BlockType &block : BlockLibrary::blocks()) {
            if (block.category != category)
                continue;

            auto *item = new QListWidgetItem(block.title, this);
            item->setData(kTypeIdRole, block.id);
            item->setToolTip(block.description);

            QPixmap swatch(12, 12);
            swatch.fill(Qt::transparent);
            QPainter p(&swatch);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(block.accent);
            p.drawRoundedRect(QRectF(0, 0, 12, 12), 3, 3);
            p.end();
            item->setIcon(QIcon(swatch));
        }
    }
}

QPixmap BlockPalette::dragPixmap(const QString &typeId) const
{
    const BlockType *block = BlockLibrary::find(typeId);
    if (!block)
        return QPixmap();

    QFont font = this->font();
    font.setBold(true);
    const QFontMetrics metrics(font);
    const int width = qMin(220, metrics.horizontalAdvance(block->title) + 28);
    const int height = 28;

    const qreal dpr = devicePixelRatioF();
    QPixmap pixmap(int(width * dpr), int(height * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x2b, 0x30, 0x3b));
    painter.drawRoundedRect(QRectF(0, 0, width, height), 6, 6);
    painter.setBrush(block->accent);
    painter.drawRoundedRect(QRectF(0, 0, 6, height), 3, 3);
    painter.setFont(font);
    painter.setPen(QColor(0xe6, 0xea, 0xf2));
    painter.drawText(QRectF(12, 0, width - 18, height), Qt::AlignVCenter | Qt::AlignLeft,
                     metrics.elidedText(block->title, Qt::ElideRight, width - 20));
    painter.end();
    return pixmap;
}

void BlockPalette::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_pressPos = eventPos(event);
    QListWidget::mousePressEvent(event);
}

void BlockPalette::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QListWidget::mouseMoveEvent(event);
        return;
    }
    if ((eventPos(event) - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
        QListWidget::mouseMoveEvent(event);
        return;
    }

    QListWidgetItem *item = itemAt(m_pressPos);
    const QString typeId = item ? item->data(kTypeIdRole).toString() : QString();
    if (typeId.isEmpty()) {
        QListWidget::mouseMoveEvent(event);
        return;
    }

    auto *mime = new QMimeData;
    mime->setData(blockMimeType(), typeId.toUtf8());
    mime->setText(typeId);

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    const QPixmap pixmap = dragPixmap(typeId);
    if (!pixmap.isNull()) {
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(12, int(pixmap.height() / pixmap.devicePixelRatio()) / 2));
    }
    drag->exec(Qt::CopyAction);
}

} // namespace nodeeditor
