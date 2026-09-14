#pragma once

namespace engine {
class World;
class CommandQueue;
}

namespace engine::era::ui {

void drawEvolutionBanner(const engine::World& world, engine::CommandQueue& commands, float screenWidth);

} // namespace engine::era::ui
