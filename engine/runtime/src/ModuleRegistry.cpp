#include "engine/runtime/ModuleRegistry.hpp"
#include "engine/world/World.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace engine {

ModuleRegistry::ModuleRegistry() = default;

ModuleRegistry::~ModuleRegistry() = default;

void ModuleRegistry::addModule(std::unique_ptr<IModule> module) {
    m_modules.push_back(std::move(module));
}

bool ModuleRegistry::build(World& world) {
    std::unordered_map<std::string_view, IModule*> moduleMap;
    for (auto& m : m_modules) {
        moduleMap[m->name()] = m.get();
    }

    std::unordered_set<std::string_view> visited;
    std::unordered_set<std::string_view> visiting;
    std::vector<IModule*> sorted;

    auto visit = [&](std::string_view name, auto& self) -> bool {
        if (visiting.count(name)) {
            std::cerr << "Circular dependency detected: " << name << std::endl;
            return false;
        }
        if (visited.count(name)) {
            return true;
        }
        
        visiting.insert(name);

        auto it = moduleMap.find(name);
        if (it != moduleMap.end()) {
            for (auto dep : it->second->dependsOn()) {
                if (!self(dep, self)) {
                    return false;
                }
            }
            sorted.push_back(it->second);
        } else {
            std::cerr << "Missing dependency: " << name << std::endl;
            return false;
        }

        visiting.erase(name);
        visited.insert(name);
        return true;
    };

    for (auto& m : m_modules) {
        if (!visit(m->name(), visit)) {
            return false;
        }
    }

    m_sortedModules = std::move(sorted);
    
    // Register types and commands
    for (auto* m : m_sortedModules) {
        m->registerTypes(world.types());
        m->registerCommands(world.commands());
    }

    // Call onAttach in topologically sorted order
    for (auto* m : m_sortedModules) {
        m->onAttach(world);
    }

    return true;
}

void ModuleRegistry::shutdown(World& world) {
    // Call onDetach in reverse topological order
    for (auto it = m_sortedModules.rbegin(); it != m_sortedModules.rend(); ++it) {
        (*it)->onDetach(world);
    }
    m_sortedModules.clear();
    m_modules.clear();
}

} // namespace engine

