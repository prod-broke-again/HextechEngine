#pragma once

#include "engine/foundation/StringHash.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::economy {

struct ResourceQuantity {
    StringHash id{};
    float amount = 0.0f;

    constexpr bool operator==(const ResourceQuantity& other) const {
        return id == other.id && amount == other.amount;
    }
};

class Inventory {
public:
    Inventory() = default;

    [[nodiscard]] float get(StringHash id) const;
    void set(StringHash id, float amount);
    void add(StringHash id, float delta);

    [[nodiscard]] bool canAfford(StringHash id, float cost) const;
    [[nodiscard]] bool canAfford(std::span<const ResourceQuantity> costs) const;

    bool tryConsume(StringHash id, float cost);
    bool tryConsume(std::span<const ResourceQuantity> costs);

    void clear();

    [[nodiscard]] const std::unordered_map<StringHash, float>& balances() const { return m_balances; }
    [[nodiscard]] std::unordered_map<StringHash, float>& balances() { return m_balances; }

    void hash(uint64_t& h) const;

private:
    std::unordered_map<StringHash, float> m_balances;
};

struct InventoryComponent {
    Inventory inventory;
};

} // namespace engine::economy
