#include <doctest/doctest.h>

#include "engine/foundation/TypeRegistry.hpp"
#include "engine/ui/EditorHistory.hpp"
#include "engine/ui/FieldAccess.hpp"

#include <entt/entt.hpp>

using namespace engine;
using namespace engine::ui;

namespace {

struct InspectSample {
    int count = 0;
    float speed = 0.0f;
    bool enabled = false;
};

} // namespace

TEST_CASE("EditorHistory - undo and redo restore field bytes") {
    TypeRegistry types;
    types.registerComponent<InspectSample>("InspectSample", 1)
        .field("count", &InspectSample::count)
        .field("speed", &InspectSample::speed)
        .field("enabled", &InspectSample::enabled);

    entt::registry registry;
    const entt::entity entity = registry.create();
    registry.emplace<InspectSample>(entity, InspectSample{1, 0.5f, false});

    const ComponentDesc* desc = types.getComponentDesc<InspectSample>();
    REQUIRE(desc != nullptr);
    REQUIRE(desc->fields.size() == 3);

    InspectSample* sample = registry.try_get<InspectSample>(entity);
    REQUIRE(sample != nullptr);

    FieldEdit edit;
    edit.entity = entity;
    edit.componentName = "InspectSample";
    edit.fieldName = "count";
    edit.type = FieldType::Int32;
    REQUIRE(captureFieldValue(sample, desc->fields[0], edit.before, edit.beforeString));
    sample->count = 9;
    REQUIRE(captureFieldValue(sample, desc->fields[0], edit.after, edit.afterString));

    EditorHistory history;
    history.push(edit);
    REQUIRE(history.canUndo());
    CHECK(sample->count == 9);

    REQUIRE(history.undo(registry, types));
    CHECK(sample->count == 1);
    REQUIRE(history.canRedo());

    REQUIRE(history.redo(registry, types));
    CHECK(sample->count == 9);
}

TEST_CASE("EditorHistory - transform diff records translation and rotation") {
    EditorHistory history;
    const entt::entity entity = static_cast<entt::entity>(7);
    commitTransformDiff(history, entity, glm::vec3{0.f}, glm::quat{1.f, 0.f, 0.f, 0.f},
                        glm::vec3{1.f, 2.f, 3.f}, glm::quat{0.f, 0.f, 0.f, 1.f});
    CHECK(history.undoCount() == 2);
}
