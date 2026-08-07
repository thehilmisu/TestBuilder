#ifndef NODEEDITOR_BLOCKPALETTE_H
#define NODEEDITOR_BLOCKPALETTE_H

#include <QListWidget>
#include <QPoint>

namespace nodeeditor {

// Source list of available blocks. Items can be dragged onto the canvas or
// double-clicked to drop one in the middle of the view.
class BlockPalette : public QListWidget
{
    Q_OBJECT

public:
    explicit BlockPalette(QWidget *parent = nullptr);

signals:
    void blockActivated(const QString &typeId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void populate();
    QPixmap dragPixmap(const QString &typeId) const;

    QPoint m_pressPos;
};

} // namespace nodeeditor

#endif // NODEEDITOR_BLOCKPALETTE_H
