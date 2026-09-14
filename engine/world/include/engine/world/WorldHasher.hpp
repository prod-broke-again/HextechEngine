#pragma once

#include "engine/world/World.hpp"
#include <string_view>
#include <vector>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <entt/entt.hpp>

namespace engine {

class WorldHasher {
public:
    using ComponentHashFn = std::function<void(const World&, entt::entity, uint64_t&)>;
    using ResourceHashFn = std::function<void(const World&, uint64_t&)>;

    static constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
    static constexpr uint64_t kFnvPrime = 1099511628211ULL;

    static void hashFnv1a(uint64_t& hash, const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= static_cast<uint64_t>(bytes[i]);
            hash *= kFnvPrime;
        }
    }

    template <typename T>
    static void hashPod(uint64_t& hash, const T& value) {
        hashFnv1a(hash, &value, sizeof(T));
    }

    template <typename T>
    static void registerComponent(std::string_view name) {
        registerComponent<T>(name, [](const T& val, uint64_t& hash) {
            hashPod(hash, val);
        });
    }

    template <typename T, typename Fn>
    static void registerComponent(std::string_view name, Fn&& fn) {
        auto& entries = componentRegistry();
        for (const auto& entry : entries) {
            if (entry.name == name) return;
        }
        constexpr uint32_t typeId = entt::type_hash<T>::value();
        entries.push_back({
            name,
            typeId,
            [fn = std::forward<Fn>(fn)](const World& world, entt::entity e, uint64_t& hash) {
                if (const auto* comp = world.registry().try_get<T>(e)) {
                    constexpr auto tid = entt::type_hash<T>::value();
                    hashPod(hash, tid);
                    fn(*comp, hash);
                }
            }
        });
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    }

    template <typename T>
    static void registerResource(std::string_view name) {
        registerResource<T>(name, [](const T& val, uint64_t& hash) {
            hashPod(hash, val);
        });
    }

    template <typename T, typename Fn>
    static void registerResource(std::string_view name, Fn&& fn) {
        auto& entries = resourceRegistry();
        for (const auto& entry : entries) {
            if (entry.name == name) return;
        }
        constexpr uint32_t typeId = entt::type_hash<T>::value();
        entries.push_back({
            name,
            typeId,
            [fn = std::forward<Fn>(fn)](const World& world, uint64_t& hash) {
                if (world.hasResource<T>()) {
                    constexpr auto tid = entt::type_hash<T>::value();
                    hashPod(hash, tid);
                    fn(world.resource<T>(), hash);
                }
            }
        });
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return a.name < b.name;
        });
    }

    static uint64_t computeHash(const World& world);
    static void clearRegistrations();

private:
    struct ComponentEntry {
        std::string_view name;
        uint32_t typeId = 0;
        ComponentHashFn hashFn;
    };

    struct ResourceEntry {
        std::string_view name;
        uint32_t typeId = 0;
        ResourceHashFn hashFn;
    };

    static std::vector<ComponentEntry>& componentRegistry();
    static std::vector<ResourceEntry>& resourceRegistry();
};

} // namespace engine
