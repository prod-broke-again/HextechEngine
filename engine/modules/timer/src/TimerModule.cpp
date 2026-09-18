#include "engine/modules/timer/TimerModule.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldHasher.hpp"

namespace engine::timer {

void TimerModule::onAttach(World& world) {
    world.types().registerComponent<TimerComponent>("TimerComponent", 1)
        .field("timer", &TimerComponent::timer);

    WorldHasher::registerComponent<TimerComponent>("TimerComponent", [](const TimerComponent& c, uint64_t& h) {
        WorldHasher::hashPod(h, c.timer.period());
        WorldHasher::hashPod(h, c.timer.elapsed());
        WorldHasher::hashPod(h, c.timer.isRepeat());
        WorldHasher::hashPod(h, c.timer.isActive());
    });
}

void TimerModule::onDetach(World& /*world*/) {
}

void TimerModule::tick(World& world) {
    auto view = world.registry().view<TimerComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TimerComponent>(entity);
        comp.timer.tick(1);
    }
}

} // namespace engine::timer
