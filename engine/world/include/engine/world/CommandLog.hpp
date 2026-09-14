#pragma once

#include "engine/world/World.hpp"
#include <entt/entt.hpp>
#include <vector>
#include <cstdint>
#include <cstring>

namespace engine {

struct RecordedCommand {
    uint64_t tick = 0;
    entt::id_type typeId = 0;
    std::vector<uint8_t> payload;
};

class CommandLog {
public:
    template <typename Cmd>
    void record(uint64_t tick, const Cmd& cmd) {
        const auto id = entt::type_hash<Cmd>::value();
        std::vector<uint8_t> bytes;
        if constexpr (sizeof(Cmd) > 0) {
            bytes.resize(sizeof(Cmd));
            std::memcpy(bytes.data(), &cmd, sizeof(Cmd));
        }
        m_entries.push_back({tick, id, std::move(bytes)});
    }

    void recordRaw(uint64_t tick, entt::id_type typeId, const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        m_entries.push_back({tick, typeId, std::vector<uint8_t>(bytes, bytes + size)});
    }

    const std::vector<RecordedCommand>& entries() const { return m_entries; }
    std::vector<RecordedCommand>& entries() { return m_entries; }

    void clear() { m_entries.clear(); }
    bool empty() const { return m_entries.empty(); }
    size_t size() const { return m_entries.size(); }

private:
    std::vector<RecordedCommand> m_entries;
};

class CommandPlayback {
public:
    explicit CommandPlayback(const CommandLog& log) : m_log(&log), m_cursor(0) {}

    void reset() {
        m_cursor = 0;
    }

    void update(World& world) {
        if (!m_log) return;
        const uint64_t currentTick = world.tick().index;
        const auto& entries = m_log->entries();

        while (m_cursor < entries.size() && entries[m_cursor].tick == currentTick) {
            const auto& entry = entries[m_cursor];
            world.commandQueue().enqueueFromBytes(
                world.commands(),
                entry.typeId,
                entry.payload.data(),
                entry.payload.size()
            );
            m_cursor++;
        }
    }

    bool finished() const {
        return !m_log || m_cursor >= m_log->entries().size();
    }

private:
    const CommandLog* m_log = nullptr;
    size_t m_cursor = 0;
};

} // namespace engine
