#pragma once

#include "engine/modules/spatial/SpatialGrid2D.hpp"
#include "engine/runtime/IModule.hpp"

namespace engine::spatial {

class SpatialModule : public IModule {
public:
    SpatialModule() = default;
    ~SpatialModule() override = default;

    std::string_view name() const override { return "SpatialModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
};

} // namespace engine::spatial
