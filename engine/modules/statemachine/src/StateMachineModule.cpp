#include "engine/modules/statemachine/StateMachineModule.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldHasher.hpp"

namespace engine::statemachine {

void StateMachineModule::onAttach(World& world) {
    world.types().registerComponent<StateMachineComponent>("StateMachineComponent", 1)
        .field("current", &StateMachineComponent::current)
        .field("previous", &StateMachineComponent::previous)
        .field("ticksInState", &StateMachineComponent::ticksInState);

    WorldHasher::registerComponent<StateMachineComponent>("StateMachineComponent", [](const StateMachineComponent& sm, uint64_t& h) {
        WorldHasher::hashPod(h, sm.current.value());
        WorldHasher::hashPod(h, sm.previous.value());
        WorldHasher::hashPod(h, sm.ticksInState);
    });
}

void StateMachineModule::onDetach(World& /*world*/) {
}

void StateMachineModule::tick(World& world) {
    auto view = world.registry().view<StateMachineComponent>();
    for (auto entity : view) {
        auto& sm = view.get<StateMachineComponent>(entity);
        sm.tick(1);
    }
}

} // namespace engine::statemachine
