#ifndef SCENARIOIO_H
#define SCENARIOIO_H

#include <QDebug>
#include <QJsonObject>
#include <QString>
#include <QStringList>

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

    // Asks for a file and rebuilds \a scene from it. Returns false if the user
    // cancelled or the file could not be read; the scene is left untouched on
    // failure. Problems that were survivable are appended to *warnings.
    bool importScenario(NodeScene *scene, QWidget *parent = nullptr,
                        QStringList *warnings = nullptr);

    // Rebuilds \a scene from an explicit path, without any dialogs.
    bool readFromFile(NodeScene *scene, const QString &filePath,
                      QStringList *problems = nullptr);

private:
    QJsonObject toJson(const NodeScene *scene, const QString &name = QString()) const;

    QString m_lastPath; // remembered between exports so the dialog reopens nearby
};

} // namespace nodeeditor

#endif // SCENARIOIO_H
