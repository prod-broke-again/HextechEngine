#pragma once

#include <cstdint>

namespace engine {

// Typed handle for resources. Slot 0 is invalid/fallback.
template <typename Tag>
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;

    constexpr bool isValid() const {
        return index != 0;
    }

    constexpr bool operator==(const Handle& other) const {
        return index == other.index && generation == other.generation;
    }

    constexpr bool operator!=(const Handle& other) const {
        return !(*this == other);
    }
};

} // namespace engine

namespace std {
    template<typename Tag>
    struct hash<engine::Handle<Tag>> {
        size_t operator()(const engine::Handle<Tag>& h) const {
            return (static_cast<size_t>(h.index) << 32) | h.generation;
        }
    };
}
