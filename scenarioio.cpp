#include "scenarioio.h"

#include "nodeeditor/connectionitem.h"
#include "nodeeditor/nodeitem.h"
#include "nodeeditor/nodescene.h"
#include "nodeeditor/portitem.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMessageBox>
#include <QSaveFile>

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
