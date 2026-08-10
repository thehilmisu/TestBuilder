#ifndef SCENARIOIO_H
#define SCENARIOIO_H

#include <QDebug>
#include <QJsonObject>
#include <QString>

class QWidget;

namespace nodeeditor {

class NodeScene;

class ScenarioIO
{
public:
    ScenarioIO();
    ~ScenarioIO() = default;

    // Returns false if the user cancelled or the write failed.
    bool exportScenario(const NodeScene *scene, QWidget *parent = nullptr);

    // Writes to an explicit path. On failure fills *errorMessage (if given).
    bool writeToFile(const NodeScene *scene, const QString &filePath,
                     QString *errorMessage = nullptr);

private:
    QJsonObject toJson(const NodeScene *scene, const QString &name = QString()) const;
    void fromJSON();

    QString m_lastPath; // remembered between exports so the dialog reopens nearby
};

} // namespace nodeeditor

#endif // SCENARIOIO_H
