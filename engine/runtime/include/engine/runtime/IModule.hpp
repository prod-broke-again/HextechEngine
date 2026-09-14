#pragma once

#include <string_view>
#include <span>

namespace engine {

class World;
class Engine;

class IModule {
public:
    virtual ~IModule() = default;

    virtual std::string_view name() const = 0;
    virtual std::span<const std::string_view> dependsOn() const { return {}; }

    virtual void onAttach(World& world) {}
    virtual void onDetach(World& world) {}
    
    // Per-tick update (30Hz logic)
    virtual void tick(World& world) {}
    
    // Per-frame update (rendering, UI, input polling)
    virtual void render(World& world, float alpha) {}
};

} // namespace engine

