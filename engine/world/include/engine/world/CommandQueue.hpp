#pragma once

#include "engine/world/CmdResult.hpp"
#include "engine/world/CommandRegistry.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace engine {

class World;

class CommandQueue {
public:
    struct QueuedItem {
        entt::id_type typeId;
        std::shared_ptr<const void> data;
    };

    template <typename Cmd>
    void enqueue(Cmd cmd) {
        const auto id = entt::type_hash<Cmd>::value();
        m_items.push_back({id, std::make_shared<Cmd>(std::move(cmd))});
    }

    void enqueueRaw(entt::id_type typeId, std::shared_ptr<const void> data) {
        m_items.push_back({typeId, std::move(data)});
    }

    bool enqueueFromBytes(const CommandRegistry& registry, entt::id_type typeId, const void* bytes, size_t size) {
        const auto* entry = registry.find(typeId);
        if (!entry) return false;
        auto ptr = entry->createFromBytes(bytes, size);
        if (!ptr) return false;
        enqueueRaw(typeId, std::move(ptr));
        return true;
    }

    bool empty() const { return m_items.empty(); }
    size_t size() const { return m_items.size(); }

    std::vector<QueuedItem> extract() {
        std::vector<QueuedItem> items;
        items.swap(m_items);
        return items;
    }

    void clear() {
        m_items.clear();
    }

private:
    std::vector<QueuedItem> m_items;
};

} // namespace engine
