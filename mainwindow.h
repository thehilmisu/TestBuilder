#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace nodeeditor {
class BlockPalette;
class NodeScene;
class NodeView;
class PropertyPanel;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void buildActions();
    void createSampleScenario();
    void updateStatus();

    nodeeditor::NodeScene *m_scene = nullptr;
    nodeeditor::NodeView *m_view = nullptr;
    nodeeditor::BlockPalette *m_palette = nullptr;
    nodeeditor::PropertyPanel *m_properties = nullptr;
};

#endif // MAINWINDOW_H
