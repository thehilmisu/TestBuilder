#ifndef RUNTIME_SCENARIOMODEL_H
#define RUNTIME_SCENARIOMODEL_H

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace nodeeditor {
class NodeScene;
}

namespace runtime {

// One executable step. This is the graph stripped of everything the editor
// needs and the runner does not: no positions, no colours, no QGraphicsItem.
struct ScenarioNode
{
    quint64 id = 0;
    QString typeId; // BlockType::id -- "expect_signal", "wait", ...
    QString label;
    QVariantMap params;
};

struct ScenarioLink
{
    quint64 fromNode = 0;
    QString fromPort;
    quint64 toNode = 0;
    QString toPort;
};

// Merges `raw` over the block type's defaults and coerces every value to the
// type its ParamSpec declares. JSON has a single number type, so a value read
// back from a file arrives as a double and would otherwise drift away from the
// spec (a QSpinBox-backed Integer showing up as 5.0, and so on).
QVariantMap coerceParams(const QString &typeId, const QVariantMap &raw);

// ---------------------------------------------------------------------------
// The runner's view of a scenario. Built either from the live canvas or from
// an exported .tbscn file, so a scenario runs identically in the editor and
// from a command line.
// ---------------------------------------------------------------------------
class ScenarioModel
{
public:
    ScenarioModel() = default;

    static ScenarioModel fromScene(const nodeeditor::NodeScene *scene);
    static ScenarioModel fromJson(const QJsonObject &root, QStringList *errors = nullptr);
    static ScenarioModel fromFile(const QString &path, QStringList *errors = nullptr);

    void addNode(const ScenarioNode &node);
    void addLink(const ScenarioLink &link);

    bool isEmpty() const { return m_nodes.isEmpty(); }
    int nodeCount() const { return m_nodes.size(); }
    int linkCount() const { return m_links.size(); }

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    // Returns nullptr when no node carries that id.
    const ScenarioNode *node(quint64 id) const;
    QVector<quint64> nodeIds() const;
    const QVector<ScenarioLink> &links() const { return m_links; }

    // Id of the single `start` node, or 0 when the graph has none.
    quint64 startNode() const;

    // Where the edge leaving (nodeId, port) lands. 0 when that port is loose --
    // which the runner treats as "the scenario ends here", not as an error.
    quint64 next(quint64 nodeId, const QString &port) const;

    // The authoring-time checks from docs/scenario-export.md. An empty list
    // means the graph is safe to run; entries are human-readable problems.
    QStringList validate() const;

private:
    static QString edgeKey(quint64 nodeId, const QString &port);

    QString m_name;
    QHash<quint64, ScenarioNode> m_nodes;
    QVector<ScenarioLink> m_links;
    QHash<QString, quint64> m_edges; // "12/pass" -> 34
};

} // namespace runtime

#endif // RUNTIME_SCENARIOMODEL_H
