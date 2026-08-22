#include "scenariomodel.h"

#include "nodeeditor/blocktypes.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQueue>
#include <QSet>

namespace testbuilder {

using nodeeditor::BlockLibrary;
using nodeeditor::BlockType;
using nodeeditor::ParamSpec;

QVariantMap coerceParams(const QString &typeId, const QVariantMap &raw)
{
    const BlockType *type = BlockLibrary::find(typeId);
    if (!type)
        return raw; // unknown block: keep what we were given rather than lose it

    // Defaults first, file values overlaid. A param added to a block type after
    // a scenario was saved then loads at its default instead of an empty value.
    QVariantMap result;
    for (const ParamSpec &spec : type->params) {
        QVariant value = raw.contains(spec.key) ? raw.value(spec.key) : spec.defaultValue;
        switch (spec.type) {
        case ParamSpec::Integer:
            value = value.toInt();
            break;
        case ParamSpec::Number:
            value = value.toDouble();
            break;
        case ParamSpec::Boolean:
            value = value.toBool();
            break;
        case ParamSpec::Choice: {
            const QString text = value.toString();
            // A choice that is no longer offered falls back to the default;
            // running against a removed option is worse than running the default.
            value = spec.choices.contains(text) ? text : spec.defaultValue;
            break;
        }
        case ParamSpec::Text:
            value = value.toString();
            break;
        }
        result.insert(spec.key, value);
    }
    return result;
}

QString ScenarioModel::edgeKey(quint64 nodeId, const QString &port)
{
    return QString::number(nodeId) + QLatin1Char('/') + port;
}

void ScenarioModel::addNode(const ScenarioNode &node)
{
    m_nodes.insert(node.id, node);
}

void ScenarioModel::addLink(const ScenarioLink &link)
{
    m_links.append(link);
    m_edges.insert(edgeKey(link.fromNode, link.fromPort), link.toNode);
}

const ScenarioNode *ScenarioModel::node(quint64 id) const
{
    auto it = m_nodes.constFind(id);
    return it == m_nodes.constEnd() ? nullptr : &it.value();
}

QVector<quint64> ScenarioModel::nodeIds() const
{
    QVector<quint64> ids;
    ids.reserve(m_nodes.size());
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
        ids.append(it.key());
    std::sort(ids.begin(), ids.end());
    return ids;
}

quint64 ScenarioModel::startNode() const
{
    for (const quint64 id : nodeIds()) {
        if (m_nodes.value(id).typeId == QLatin1String("start"))
            return id;
    }
    return 0;
}

quint64 ScenarioModel::next(quint64 nodeId, const QString &port) const
{
    return m_edges.value(edgeKey(nodeId, port), 0);
}

ScenarioModel ScenarioModel::fromJson(const QJsonObject &root, QStringList *errors)
{
    ScenarioModel model;
    QStringList problems;

    const QString format = root.value(QStringLiteral("format")).toString();
    if (format != QLatin1String("testbuilder.scenario"))
        problems << QStringLiteral("Not a TestBuilder scenario (format = '%1').").arg(format);

    const int version = root.value(QStringLiteral("formatVersion")).toInt();
    if (version > 1)
        problems << QStringLiteral("File was written by a newer version (formatVersion %1); "
                                   "unknown fields are ignored.").arg(version);

    model.setName(root.value(QStringLiteral("name")).toString());

    for (const QJsonValue &value : root.value(QStringLiteral("nodes")).toArray()) {
        const QJsonObject object = value.toObject();
        ScenarioNode node;
        node.id = quint64(object.value(QStringLiteral("id")).toDouble());
        node.typeId = object.value(QStringLiteral("type")).toString();
        node.label = object.value(QStringLiteral("label")).toString();

        if (node.id == 0) {
            problems << QStringLiteral("Skipped a node without an id.");
            continue;
        }
        if (!BlockLibrary::find(node.typeId)) {
            problems << QStringLiteral("Node %1 has unknown block type '%2'; it will not run.")
                            .arg(node.id).arg(node.typeId);
        }
        node.params = coerceParams(node.typeId,
                                   object.value(QStringLiteral("params")).toObject().toVariantMap());
        model.addNode(node);
    }

    for (const QJsonValue &value : root.value(QStringLiteral("links")).toArray()) {
        const QJsonObject object = value.toObject();
        const QJsonObject from = object.value(QStringLiteral("from")).toObject();
        const QJsonObject to = object.value(QStringLiteral("to")).toObject();

        ScenarioLink link;
        link.fromNode = quint64(from.value(QStringLiteral("node")).toDouble());
        link.fromPort = from.value(QStringLiteral("port")).toString();
        link.toNode = quint64(to.value(QStringLiteral("node")).toDouble());
        link.toPort = to.value(QStringLiteral("port")).toString();

        // A link to a node that was skipped above would strand the runner.
        if (!model.node(link.fromNode) || !model.node(link.toNode)) {
            problems << QStringLiteral("Dropped a link between missing nodes %1 -> %2.")
                            .arg(link.fromNode).arg(link.toNode);
            continue;
        }
        model.addLink(link);
    }

    if (errors)
        *errors = problems;
    return model;
}

ScenarioModel ScenarioModel::fromFile(const QString &path, QStringList *errors)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errors)
            *errors = QStringList{QStringLiteral("Could not open %1: %2").arg(path, file.errorString())};
        return ScenarioModel();
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errors)
            *errors = QStringList{QStringLiteral("%1 is not valid JSON: %2")
                                      .arg(path, parseError.errorString())};
        return ScenarioModel();
    }

    ScenarioModel model = fromJson(document.object(), errors);
    if (model.name().isEmpty())
        model.setName(QFileInfo(path).completeBaseName());
    return model;
}

QStringList ScenarioModel::validate() const
{
    QStringList problems;

    if (m_nodes.isEmpty()) {
        problems << QStringLiteral("The scenario is empty.");
        return problems;
    }

    QVector<quint64> starts;
    for (const quint64 id : nodeIds()) {
        if (m_nodes.value(id).typeId == QLatin1String("start"))
            starts.append(id);
    }
    if (starts.isEmpty())
        problems << QStringLiteral("No Start block -- the runner has nowhere to begin.");
    else if (starts.size() > 1)
        problems << QStringLiteral("%1 Start blocks; exactly one is allowed.").arg(starts.size());

    // Which node ids are the target of some link, so we can spot loose inputs.
    QSet<quint64> hasIncoming;
    for (const ScenarioLink &link : m_links)
        hasIncoming.insert(link.toNode);

    for (const quint64 id : nodeIds()) {
        const ScenarioNode &node = m_nodes.value(id);
        const QString name = node.label.isEmpty() ? node.typeId : node.label;
        const BlockType *type = BlockLibrary::find(node.typeId);
        if (!type) {
            problems << QStringLiteral("'%1' (node %2) is an unknown block type.").arg(name).arg(id);
            continue;
        }

        if (!type->inputs.isEmpty() && !hasIncoming.contains(id))
            problems << QStringLiteral("'%1' (node %2) has nothing wired into its input.").arg(name).arg(id);

        for (const nodeeditor::PortSpec &port : type->outputs) {
            if (next(id, port.name) == 0) {
                problems << QStringLiteral("'%1' (node %2) leaves its '%3' output unconnected.")
                                .arg(name).arg(id).arg(port.name);
            }
        }
    }

    // Unreachable nodes: run a BFS from start over the links.
    if (starts.size() == 1) {
        QSet<quint64> seen{starts.first()};
        QQueue<quint64> queue;
        queue.enqueue(starts.first());
        while (!queue.isEmpty()) {
            const quint64 current = queue.dequeue();
            for (const ScenarioLink &link : m_links) {
                if (link.fromNode == current && !seen.contains(link.toNode)) {
                    seen.insert(link.toNode);
                    queue.enqueue(link.toNode);
                }
            }
        }
        for (const quint64 id : nodeIds()) {
            if (!seen.contains(id)) {
                const ScenarioNode &node = m_nodes.value(id);
                problems << QStringLiteral("'%1' (node %2) is unreachable from Start.")
                                .arg(node.label.isEmpty() ? node.typeId : node.label).arg(id);
            }
        }
    }

    return problems;
}

} // namespace testbuilder
