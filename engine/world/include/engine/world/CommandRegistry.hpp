#pragma once

#include "engine/world/CmdResult.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>

namespace engine {

class World;

class CommandRegistry {
public:
    struct IEntry {
        virtual ~IEntry() = default;
        virtual CmdResult validate(const World& world, const void* cmdPtr) const = 0;
        virtual void apply(World& world, const void* cmdPtr) const = 0;
        virtual std::shared_ptr<const void> createFromBytes(const void* bytes, size_t size) const = 0;
    };

    template <typename Cmd>
    struct Entry final : IEntry {
        CmdResult (*validateFn)(const World&, const Cmd&);
        void (*applyFn)(World&, const Cmd&);

        Entry(CmdResult (*v)(const World&, const Cmd&), void (*a)(World&, const Cmd&))
            : validateFn(v), applyFn(a) {}

        CmdResult validate(const World& world, const void* cmdPtr) const override {
            return validateFn(world, *static_cast<const Cmd*>(cmdPtr));
        }

        void apply(World& world, const void* cmdPtr) const override {
            applyFn(world, *static_cast<const Cmd*>(cmdPtr));
        }

        std::shared_ptr<const void> createFromBytes(const void* bytes, size_t size) const override {
            if constexpr (sizeof(Cmd) > 0) {
                if (size < sizeof(Cmd)) return nullptr;
                auto ptr = std::make_shared<Cmd>();
                std::memcpy(ptr.get(), bytes, sizeof(Cmd));
                return ptr;
            } else {
                return std::make_shared<Cmd>();
            }
        }
    };

    template <typename Cmd>
    void registerCommand(CmdResult (*validateFn)(const World&, const Cmd&),
                         void (*applyFn)(World&, const Cmd&)) {
        const auto id = entt::type_hash<Cmd>::value();
        m_entries[id] = std::make_unique<Entry<Cmd>>(validateFn, applyFn);
    }

    template <typename Cmd>
    bool isRegistered() const {
        const auto id = entt::type_hash<Cmd>::value();
        return m_entries.find(id) != m_entries.end();
    }

    const IEntry* find(entt::id_type typeId) const {
        auto it = m_entries.find(typeId);
        return it != m_entries.end() ? it->second.get() : nullptr;
    }

    template <typename Cmd>
    CmdResult validate(const World& world, const Cmd& cmd) const {
        const auto* entry = find(entt::type_hash<Cmd>::value());
        if (!entry) {
            return CmdResult::fail(CmdStatus::UnknownCommand, "Command type is not registered");
        }
        return entry->validate(world, &cmd);
    }

    template <typename Cmd>
    bool execute(World& world, const Cmd& cmd) const {
        const auto* entry = find(entt::type_hash<Cmd>::value());
        if (!entry) {
            return false;
        }
        CmdResult res = entry->validate(world, &cmd);
        if (res.ok()) {
            entry->apply(world, &cmd);
            return true;
        }
        return false;
    }

private:
    std::unordered_map<entt::id_type, std::unique_ptr<IEntry>> m_entries;
};

} // namespace engine
