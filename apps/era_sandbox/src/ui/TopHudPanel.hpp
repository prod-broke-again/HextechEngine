#pragma once

namespace engine {
class World;
class CommandQueue;
}

namespace engine::era::ui {

void drawTopHud(const engine::World& world, engine::CommandQueue& commands);

} // namespace engine::era::ui
