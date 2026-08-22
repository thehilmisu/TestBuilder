#include "scenemodel.h"

#include "connectionitem.h"
#include "nodeitem.h"
#include "nodescene.h"
#include "portitem.h"

namespace nodeeditor {

testbuilder::ScenarioModel toScenarioModel(const NodeScene *scene)
{
    testbuilder::ScenarioModel model;
    if (!scene)
        return model;

    for (const NodeItem *item : scene->nodes()) {
        testbuilder::ScenarioNode node;
        node.id = item->getId();
        node.typeId = item->typeId();
        node.label = item->title();
        node.params = testbuilder::coerceParams(node.typeId, item->params());
        model.addNode(node);
    }

    for (const ConnectionItem *connection : scene->connections()) {
        const PortItem *from = connection->source();
        const PortItem *to = connection->dest();
        if (!from || !to || !from->node() || !to->node())
            continue; // half-dragged link
        model.addLink({from->node()->getId(), from->name(), to->node()->getId(), to->name()});
    }

    return model;
}

} // namespace nodeeditor
