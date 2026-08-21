#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QAction;

namespace runtime {
class LinTransport;
class ScenarioRunner;
}

namespace nodeeditor {
class BlockPalette;
class NodeItem;
class NodeScene;
class NodeView;
class PropertyPanel;
class RunPanel;
class ScenarioIO;
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
    void buildRunner();
    void createSampleScenario();
    void updateStatus();

    void runScenario();
    void clearRunHighlights();

    nodeeditor::NodeScene *m_scene = nullptr;
    nodeeditor::NodeView *m_view = nullptr;
    nodeeditor::BlockPalette *m_palette = nullptr;
    nodeeditor::PropertyPanel *m_properties = nullptr;
    nodeeditor::ScenarioIO *m_scenarioIO = nullptr;
    nodeeditor::RunPanel *m_runPanel = nullptr;

    runtime::LinTransport *m_transport = nullptr;
    runtime::ScenarioRunner *m_runner = nullptr;

    QAction *m_runAction = nullptr;
    QAction *m_stopAction = nullptr;

    // The block the runner is standing on, so it can be un-highlighted when it
    // moves on without walking the whole scene.
    nodeeditor::NodeItem *m_activeNode = nullptr;
};

#endif // MAINWINDOW_H
