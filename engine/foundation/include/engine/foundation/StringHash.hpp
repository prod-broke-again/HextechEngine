#pragma once

#include <cstdint>
#include <string_view>

namespace engine {

class StringHash {
public:
    // FNV-1a constants for 32-bit
    static constexpr uint32_t kOffsetBasis = 0x811C9DC5;
    static constexpr uint32_t kPrime = 0x01000193;

    constexpr StringHash() : m_hash(0) {}
    constexpr StringHash(uint32_t hash) : m_hash(hash) {}
    
    constexpr StringHash(std::string_view str) : m_hash(hashStr(str)) {}

    constexpr uint32_t value() const { return m_hash; }

    constexpr bool operator==(const StringHash& other) const {
        return m_hash == other.m_hash;
    }

    constexpr bool operator!=(const StringHash& other) const {
        return m_hash != other.m_hash;
    }

    constexpr bool operator<(const StringHash& other) const {
        return m_hash < other.m_hash;
    }

    constexpr bool operator<=(const StringHash& other) const {
        return m_hash <= other.m_hash;
    }

    constexpr bool operator>(const StringHash& other) const {
        return m_hash > other.m_hash;
    }

    constexpr bool operator>=(const StringHash& other) const {
        return m_hash >= other.m_hash;
    }

private:
    constexpr static uint32_t hashStr(std::string_view str) {
        uint32_t hash = kOffsetBasis;
        for (char c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= kPrime;
        }
        return hash;
    }

    uint32_t m_hash;
};

constexpr inline StringHash operator""_sh(const char* str, size_t len) {
    return StringHash(std::string_view(str, len));
}

} // namespace engine


// std::hash specialization so we can use it in unordered_map if needed (though discouraged in simulation, OK in editor)
namespace std {
    template<>
    struct hash<engine::StringHash> {
        size_t operator()(const engine::StringHash& sh) const {
            return static_cast<size_t>(sh.value());
        }
    };
}
