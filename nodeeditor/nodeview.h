#ifndef NODEEDITOR_NODEVIEW_H
#define NODEEDITOR_NODEVIEW_H

#include <QGraphicsView>
#include <QPoint>

namespace nodeeditor {

class NodeScene;

// Canvas viewport: wheel zoom, middle-button panning, rubber-band selection.
class NodeView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit NodeView(NodeScene *scene, QWidget *parent = nullptr);

    NodeScene *nodeScene() const { return m_scene; }

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitGraphInView();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void applyZoom(qreal factor);

    NodeScene *m_scene;
    bool m_panning = false;
    QPoint m_lastPanPos;
};

} // namespace nodeeditor

#endif // NODEEDITOR_NODEVIEW_H
