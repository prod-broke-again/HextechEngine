#pragma once

#include "engine/runtime/IModule.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace engine {

class World;

class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    void addModule(std::unique_ptr<IModule> module);

    bool build(World& world);
    void shutdown(World& world);

    const std::vector<IModule*>& sortedModules() const { return m_sortedModules; }

private:
    std::vector<std::unique_ptr<IModule>> m_modules;
    std::vector<IModule*> m_sortedModules;
};

} // namespace engine

