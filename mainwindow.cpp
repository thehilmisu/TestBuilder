#include "mainwindow.h"

#include "nodeeditor/blockpalette.h"
#include "nodeeditor/blocktypes.h"
#include "nodeeditor/nodeitem.h"
#include "nodeeditor/nodescene.h"
#include "nodeeditor/nodeview.h"
#include "scenarioio.h"
#include "nodeeditor/portitem.h"
#include "nodeeditor/propertypanel.h"
#include "runpanel.h"
#include "runtime/scenariomodel.h"
#include "runtime/scenariorunner.h"

#include <QAction>
#include <QDockWidget>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QToolBar>

using namespace nodeeditor;
using runtime::ScenarioRunner;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("TestBuilder - Scenario Editor"));
    resize(1280, 800);

    buildUi();
    buildRunner();
    buildActions();
    createSampleScenario();
    updateStatus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    m_scene = new NodeScene(this);
    m_view = new NodeView(m_scene, this);
    m_scenarioIO = new ScenarioIO();

    setCentralWidget(m_view);

    m_palette = new BlockPalette(this);
    auto *paletteDock = new QDockWidget(tr("Blocks"), this);
    paletteDock->setObjectName(QStringLiteral("blocksDock"));
    paletteDock->setWidget(m_palette);
    paletteDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, paletteDock);

    m_properties = new PropertyPanel(this);
    auto *propertiesDock = new QDockWidget(tr("Properties"), this);
    propertiesDock->setObjectName(QStringLiteral("propertiesDock"));
    propertiesDock->setWidget(m_properties);
    propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    resizeDocks({paletteDock, propertiesDock}, {220, 280}, Qt::Horizontal);

    m_runPanel = new RunPanel(this);
    auto *runDock = new QDockWidget(tr("Run"), this);
    runDock->setObjectName(QStringLiteral("runDock"));
    runDock->setWidget(m_runPanel);
    runDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::BottomDockWidgetArea, runDock);
    resizeDocks({runDock}, {200}, Qt::Vertical);

    connect(m_scene, &NodeScene::nodeSelected, m_properties, &PropertyPanel::setNode);
    connect(m_scene, &NodeScene::nodeAboutToBeRemoved, m_properties, &PropertyPanel::forgetNode);
    connect(m_scene, &NodeScene::graphChanged, this, &MainWindow::updateStatus);

    // Double-clicking a palette entry drops the block in the middle of the view.
    connect(m_palette, &BlockPalette::blockActivated, this, [this](const QString &typeId) {
        const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
        if (NodeItem *node = m_scene->addNode(typeId, center - QPointF(90, 40))) {
            m_scene->clearSelection();
            node->setSelected(true);
        }
    });

    statusBar()->showMessage(tr("Drag a block from the left onto the canvas, then drag from an "
                                "output port to an input port to wire it up."));
}

void MainWindow::buildRunner()
{
    // Swap this for your hardware-backed LinTransport; nothing else changes.
    m_transport = new runtime::SimulatedLinTransport(this);

    m_runner = new ScenarioRunner(this);
    m_runner->setTransport(m_transport);
    m_runPanel->attach(m_runner);

    // Follow the run on the canvas: the block being executed gets the active
    // halo, the ones behind it keep a dimmer one.
    connect(m_runner, &ScenarioRunner::nodeEntered, this, [this](quint64 nodeId) {
        if (m_activeNode)
            m_activeNode->setRunState(NodeItem::RunState::Visited);
        m_activeNode = nullptr;
        for (NodeItem *node : m_scene->nodes()) {
            if (node->getId() == nodeId) {
                node->setRunState(NodeItem::RunState::Active);
                m_activeNode = node;
                break;
            }
        }
    });

    connect(m_runner, &ScenarioRunner::finished, this,
            [this](ScenarioRunner::Verdict verdict, const QString &reason) {
        if (m_activeNode) {
            const bool bad = verdict != ScenarioRunner::Verdict::Pass;
            m_activeNode->setRunState(bad ? NodeItem::RunState::Failed
                                          : NodeItem::RunState::Visited);
        }
        m_runAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        statusBar()->showMessage(reason, 8000);
    });

    connect(m_runner, &ScenarioRunner::stateChanged, this, [this](ScenarioRunner::State state) {
        const bool busy = state == ScenarioRunner::State::Running
                          || state == ScenarioRunner::State::Waiting;
        m_runAction->setEnabled(!busy);
        m_stopAction->setEnabled(busy);
    });
}

void MainWindow::runScenario()
{
    clearRunHighlights();

    QStringList problems;
    // The canvas is the source of truth for a run; there is no need to save
    // first, and an unsaved experiment runs exactly like a saved one.
    if (!m_runner->load(runtime::ScenarioModel::fromScene(m_scene), &problems)) {
        QMessageBox::warning(this, tr("Cannot run"),
                             tr("This scenario cannot be executed:\n\n%1")
                                 .arg(problems.join(QLatin1Char('\n'))));
        return;
    }

    // Everything else validate() found is a warning: a loose branch simply ends
    // the run when it is reached, which is worth knowing but not worth blocking.
    if (!problems.isEmpty()) {
        const auto answer = QMessageBox::warning(
            this, tr("Run anyway?"),
            tr("The scenario has problems:\n\n%1\n\nRun it anyway?")
                .arg(problems.join(QLatin1Char('\n'))),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return;
    }

    m_runner->setStepDelayMs(m_runPanel->stepDelayMs());
    m_runner->start();
}

void MainWindow::clearRunHighlights()
{
    for (NodeItem *node : m_scene->nodes())
        node->setRunState(NodeItem::RunState::Idle);
    m_activeNode = nullptr;
}

void MainWindow::buildActions()
{
    auto *newAction = new QAction(tr("&New Test"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] {
        m_scene->clearGraph();
        m_properties->setNode(nullptr);
        clearRunHighlights();
        updateStatus();
    });

    auto *deleteAction = new QAction(tr("&Delete Selection"), this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this] { m_scene->deleteSelection(); });

    auto *selectAllAction = new QAction(tr("Select &All"), this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this] {
        for (NodeItem *node : m_scene->nodes())
            node->setSelected(true);
    });

    auto *zoomInAction = new QAction(tr("Zoom &In"), this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, m_view, &NodeView::zoomIn);

    auto *zoomOutAction = new QAction(tr("Zoom &Out"), this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, m_view, &NodeView::zoomOut);

    auto *resetZoomAction = new QAction(tr("&Reset Zoom"), this);
    resetZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetZoomAction, &QAction::triggered, m_view, &NodeView::resetZoom);

    auto *fitAction = new QAction(tr("&Fit to Screen"), this);
    fitAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction, &QAction::triggered, m_view, &NodeView::fitGraphInView);

    auto *openAction = new QAction(tr("&Open..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        if (m_scenarioIO->importScenario(m_scene, this)) {
            m_properties->setNode(nullptr);
            clearRunHighlights();
            m_view->fitGraphInView();
            updateStatus();
        }
    });

    m_runAction = new QAction(tr("&Run"), this);
    m_runAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(m_runAction, &QAction::triggered, this, &MainWindow::runScenario);

    m_stopAction = new QAction(tr("S&top"), this);
    m_stopAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Period));
    m_stopAction->setEnabled(false);
    connect(m_stopAction, &QAction::triggered, m_runner, &ScenarioRunner::stop);

    auto *exportAction = new QAction(tr("&Export"), this);
    exportAction->setShortcut(QKeySequence(Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, [this] {
        // pass the scene to the IO; `this` parents the file dialog to the window
        m_scenarioIO->exportScenario(m_scene, this);
    });

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(exportAction);

    QMenu *runMenu = menuBar()->addMenu(tr("&Run"));
    runMenu->addAction(m_runAction);
    runMenu->addAction(m_stopAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(deleteAction);
    editMenu->addAction(selectAllAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(resetZoomAction);
    viewMenu->addAction(fitAction);

    auto *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->addAction(newAction);
    toolBar->addAction(openAction);
    toolBar->addAction(exportAction);
    toolBar->addSeparator();
    toolBar->addAction(m_runAction);
    toolBar->addAction(m_stopAction);
    toolBar->addSeparator();
    toolBar->addAction(deleteAction);
    toolBar->addSeparator();
    toolBar->addAction(zoomInAction);
    toolBar->addAction(zoomOutAction);
    toolBar->addAction(resetZoomAction);
    toolBar->addAction(fitAction);
}

void MainWindow::createSampleScenario()
{
    // A small starter graph so the canvas is not empty on first launch.
    NodeItem *start = m_scene->addNode(QStringLiteral("start"), QPointF(-380, -60));
    NodeItem *check = m_scene->addNode(QStringLiteral("expect_signal"), QPointF(350, -100));
    NodeItem *pass = m_scene->addNode(QStringLiteral("end"), QPointF(620, -140));
    NodeItem *fail = m_scene->addNode(QStringLiteral("end"), QPointF(620, -10));

    if (!start || !check || !pass || !fail)
        return;

    fail->setTitle(tr("End (failed)"));
    fail->setParam(QStringLiteral("verdict"), QStringLiteral("Fail"));

    m_scene->connectPorts(start->outputs().at(0), check->inputs().at(0));
    m_scene->connectPorts(check->outputs().at(0), pass->inputs().at(0));
    m_scene->connectPorts(check->outputs().at(1), fail->inputs().at(0));


    m_view->fitGraphInView();
}

void MainWindow::updateStatus()
{
    statusBar()->showMessage(tr("%n block(s)", nullptr, m_scene->nodes().size()) +
                             tr(" - %n link(s)", nullptr, m_scene->connections().size()));
}
