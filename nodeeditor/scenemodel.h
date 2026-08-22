#ifndef NODEEDITOR_SCENEMODEL_H
#define NODEEDITOR_SCENEMODEL_H

#include "runtime/scenariomodel.h"

namespace nodeeditor {

class NodeScene;

// Snapshots the live canvas as a runnable model.
//
// This is the only place the editor and the runtime meet, and it lives on the
// editor's side of the line on purpose: everything in runtime/ then builds
// without QtWidgets, so a host application can link the engine without
// dragging the node editor in with it.
testbuilder::ScenarioModel toScenarioModel(const NodeScene *scene);

} // namespace nodeeditor

#endif // NODEEDITOR_SCENEMODEL_H
