#include "mainwindow.h"

#include "nodeeditor/blockpalette.h"
#include "nodeeditor/blocktypes.h"
#include "nodeeditor/nodeitem.h"
#include "nodeeditor/nodescene.h"
#include "nodeeditor/nodeview.h"
#include "nodeeditor/portitem.h"
#include "nodeeditor/propertypanel.h"

#include <QAction>
#include <QDockWidget>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

using namespace nodeeditor;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("TestBuilder - Scenario Editor"));
    resize(1280, 800);

    buildUi();
    buildActions();
    createSampleScenario();
    updateStatus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    m_scene = new NodeScene(this);
    m_view = new NodeView(m_scene, this);
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

void MainWindow::buildActions()
{
    auto *newAction = new QAction(tr("&New Scenario"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] {
        m_scene->clearGraph();
        m_properties->setNode(nullptr);
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

    auto *fitAction = new QAction(tr("&Fit Scenario"), this);
    fitAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction, &QAction::triggered, m_view, &NodeView::fitGraphInView);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);

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
    NodeItem *home = m_scene->addNode(QStringLiteral("actuator_home"), QPointF(-160, -80));
    NodeItem *move = m_scene->addNode(QStringLiteral("actuator_move"), QPointF(90, -110));
    NodeItem *check = m_scene->addNode(QStringLiteral("expect_signal"), QPointF(350, -100));
    NodeItem *pass = m_scene->addNode(QStringLiteral("end"), QPointF(620, -140));
    NodeItem *fail = m_scene->addNode(QStringLiteral("end"), QPointF(620, -10));

    if (!start || !home || !move || !check || !pass || !fail)
        return;

    fail->setTitle(tr("End (failed)"));
    fail->setParam(QStringLiteral("verdict"), QStringLiteral("Fail"));
    move->setParam(QStringLiteral("position"), 1200);

    m_scene->connectPorts(start->outputs().at(0), home->inputs().at(0));
    m_scene->connectPorts(home->outputs().at(0), move->inputs().at(0));
    m_scene->connectPorts(move->outputs().at(0), check->inputs().at(0));
    m_scene->connectPorts(check->outputs().at(0), pass->inputs().at(0));
    m_scene->connectPorts(check->outputs().at(1), fail->inputs().at(0));

    m_view->fitGraphInView();
}

void MainWindow::updateStatus()
{
    statusBar()->showMessage(tr("%n block(s)", nullptr, m_scene->nodes().size()) +
                             tr(" - %n link(s)", nullptr, m_scene->connections().size()));
}
