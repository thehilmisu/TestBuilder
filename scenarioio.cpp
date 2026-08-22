#include "scenarioio.h"

#include "nodeeditor/connectionitem.h"
#include "nodeeditor/nodeitem.h"
#include "nodeeditor/nodescene.h"
#include "nodeeditor/portitem.h"
#include "runtime/scenariomodel.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMessageBox>
#include <QSaveFile>
#include <QHash>

#include <algorithm>

namespace nodeeditor {

ScenarioIO::ScenarioIO() {}

QJsonObject ScenarioIO::toJson(const NodeScene *scene, const QString &name) const
{
    QVector<NodeItem *> nodeList = scene->nodes();

    // nodes() comes from items(), which returns stacking order -- that shifts as
    // the user clicks around. Sort by id so an unchanged graph saves identically.
    std::sort(nodeList.begin(), nodeList.end(), [](const NodeItem *a, const NodeItem *b) {
        return a->getId() < b->getId();
    });

    QJsonArray nodes;
    for (const NodeItem *node : nodeList) {
        QJsonObject nodeObj;
        nodeObj[QStringLiteral("id")] = qint64(node->getId());
        // typeId() is the BlockType id ("expect_signal"); type() is the
        // QGraphicsItem type constant and must not go in the file.
        nodeObj[QStringLiteral("type")] = node->typeId();
        nodeObj[QStringLiteral("label")] = node->title();
        nodeObj[QStringLiteral("params")] = QJsonObject::fromVariantMap(node->params());

        QJsonObject ui;
        ui[QStringLiteral("x")] = node->pos().x();
        ui[QStringLiteral("y")] = node->pos().y();
        nodeObj[QStringLiteral("ui")] = ui;

        nodes.append(nodeObj);
    }

    struct Edge
    {
        quint64 fromNode;
        QString fromPort;
        quint64 toNode;
        QString toPort;
    };

    QVector<Edge> edges;
    const QVector<ConnectionItem *> connections = scene->connections();
    for (const ConnectionItem *connection : connections) {
        const PortItem *from = connection->source();
        const PortItem *to = connection->dest();
        // A link still being dragged has one loose end; there is nothing to persist.
        if (!from || !to || !from->node() || !to->node())
            continue;
        edges.append({from->node()->getId(), from->name(), to->node()->getId(), to->name()});
    }

    std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) {
        if (a.fromNode != b.fromNode)
            return a.fromNode < b.fromNode;
        if (a.fromPort != b.fromPort)
            return a.fromPort < b.fromPort;
        return a.toNode < b.toNode;
    });

    QJsonArray links;
    for (const Edge &edge : edges) {
        QJsonObject fromObj;
        fromObj[QStringLiteral("node")] = qint64(edge.fromNode);
        fromObj[QStringLiteral("port")] = edge.fromPort;

        QJsonObject toObj;
        toObj[QStringLiteral("node")] = qint64(edge.toNode);
        toObj[QStringLiteral("port")] = edge.toPort;

        QJsonObject linkObj;
        linkObj[QStringLiteral("from")] = fromObj;
        linkObj[QStringLiteral("to")] = toObj;
        links.append(linkObj);
    }

    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("testbuilder.scenario");
    root[QStringLiteral("formatVersion")] = 1;
    root[QStringLiteral("name")] = name.isEmpty() ? QStringLiteral("Untitled") : name;
    root[QStringLiteral("nodes")] = nodes;
    root[QStringLiteral("links")] = links;
    return root;
}

bool ScenarioIO::readFromFile(NodeScene *scene, const QString &filePath, QStringList *problems)
{
    if (!scene) {
        if (problems)
            problems->append(QObject::tr("No scene to load into."));
        return false;
    }

    QStringList collected;
    // The model layer already parses this format, coerces params against their
    // ParamSpec and reports what it had to skip; here we only add back the
    // editor-only bits it deliberately drops -- the node positions.
    const testbuilder::ScenarioModel model = testbuilder::ScenarioModel::fromFile(filePath, &collected);
    if (model.isEmpty()) {
        if (collected.isEmpty())
            collected.append(QObject::tr("%1 contains no blocks.").arg(filePath));
        if (problems)
            *problems = collected;
        return false;
    }

    QHash<quint64, QPointF> positions;
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
            for (const QJsonValue &value : root.value(QStringLiteral("nodes")).toArray()) {
                const QJsonObject object = value.toObject();
                const QJsonObject ui = object.value(QStringLiteral("ui")).toObject();
                positions.insert(quint64(object.value(QStringLiteral("id")).toDouble()),
                                 QPointF(ui.value(QStringLiteral("x")).toDouble(),
                                         ui.value(QStringLiteral("y")).toDouble()));
            }
        }
    }

    // Nothing above this point touched the scene, so a bad file leaves the
    // canvas as it was.
    scene->clearGraph();

    QHash<quint64, NodeItem *> byId;
    for (const quint64 id : model.nodeIds()) {
        const testbuilder::ScenarioNode *source = model.node(id);
        NodeItem *node = scene->addNode(source->typeId, positions.value(id));
        if (!node) {
            collected.append(QObject::tr("Skipped node %1: unknown block type '%2'.")
                                 .arg(id).arg(source->typeId));
            continue;
        }
        node->setId(id);
        if (!source->label.isEmpty())
            node->setTitle(source->label);
        for (auto it = source->params.constBegin(); it != source->params.constEnd(); ++it)
            node->setParam(it.key(), it.value());
        byId.insert(id, node);
    }

    // Second pass: a link may reference a node that appears later in the file.
    for (const testbuilder::ScenarioLink &link : model.links()) {
        NodeItem *from = byId.value(link.fromNode);
        NodeItem *to = byId.value(link.toNode);
        if (!from || !to)
            continue; // its node was skipped above and already reported

        PortItem *fromPort = nullptr;
        for (PortItem *port : from->outputs()) {
            if (port->name() == link.fromPort) {
                fromPort = port;
                break;
            }
        }
        PortItem *toPort = nullptr;
        for (PortItem *port : to->inputs()) {
            if (port->name() == link.toPort) {
                toPort = port;
                break;
            }
        }

        if (!fromPort || !toPort) {
            // Ports are matched by name precisely so reordering them in
            // blocktypes.cpp cannot silently rewire a branch; a rename shows up
            // here instead.
            collected.append(QObject::tr("Dropped link %1:%2 -> %3:%4: no such port.")
                                 .arg(link.fromNode).arg(link.fromPort)
                                 .arg(link.toNode).arg(link.toPort));
            continue;
        }
        scene->connectPorts(fromPort, toPort);
    }

    if (problems)
        *problems = collected;
    return true;
}

bool ScenarioIO::importScenario(NodeScene *scene, QWidget *parent, QStringList *warnings)
{
    const QString startPath = m_lastPath.isEmpty()
                                  ? QDir::homePath() + QStringLiteral("/scenario.tbscn")
                                  : m_lastPath;

    const QString filePath = QFileDialog::getOpenFileName(
        parent, QObject::tr("Open Scenario"), startPath,
        QObject::tr("Scenario (*.tbscn);;JSON (*.json);;All files (*)"));

    if (filePath.isEmpty())
        return false; // cancelled

    QStringList problems;
    if (!readFromFile(scene, filePath, &problems)) {
        QMessageBox::warning(parent, QObject::tr("Open failed"),
                             QObject::tr("Could not load %1:\n%2")
                                 .arg(filePath, problems.join(QLatin1Char('\n'))));
        return false;
    }

    m_lastPath = filePath;
    if (warnings)
        *warnings = problems;
    else if (!problems.isEmpty())
        QMessageBox::information(parent, QObject::tr("Scenario loaded with problems"),
                                 problems.join(QLatin1Char('\n')));
    return true;
}

bool ScenarioIO::writeToFile(const NodeScene *scene, const QString &filePath, QString *errorMessage)
{
    if (!scene) {
        if (errorMessage)
            *errorMessage = QObject::tr("No scenario to export.");
        return false;
    }

    // QSaveFile writes to a temporary and renames on commit(), so a crash or a
    // full disk mid-write cannot destroy the previously exported scenario.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    const QJsonDocument document(toJson(scene, QFileInfo(filePath).completeBaseName()));
    if (file.write(document.toJson(QJsonDocument::Indented)) == -1) {
        if (errorMessage)
            *errorMessage = file.errorString();
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool ScenarioIO::exportScenario(const NodeScene *scene, QWidget *parent)
{
    if (!scene)
        return false;

    const QString startPath = m_lastPath.isEmpty()
                                  ? QDir::homePath() + QStringLiteral("/scenario.tbscn")
                                  : m_lastPath;

    QString filePath = QFileDialog::getSaveFileName(
        parent, QObject::tr("Export Scenario"), startPath,
        QObject::tr("Scenario (*.tbscn);;JSON (*.json);;All files (*)"));

    if (filePath.isEmpty())
        return false; // cancelled

    // The dialog does not reliably append the filter's extension on every platform.
    if (QFileInfo(filePath).suffix().isEmpty())
        filePath += QStringLiteral(".tbscn");

    QString error;
    if (!writeToFile(scene, filePath, &error)) {
        QMessageBox::warning(parent, QObject::tr("Export failed"),
                             QObject::tr("Could not write %1:\n%2").arg(filePath, error));
        return false;
    }

    m_lastPath = filePath;
    return true;
}

} // namespace nodeeditor
