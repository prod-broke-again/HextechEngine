#include "engine/modules/economy/Inventory.hpp"
#include "engine/world/WorldHasher.hpp"

#include <algorithm>

namespace engine::economy {

float Inventory::get(StringHash id) const {
    auto it = m_balances.find(id);
    return (it != m_balances.end()) ? it->second : 0.0f;
}

void Inventory::set(StringHash id, float amount) {
    m_balances[id] = amount;
}

void Inventory::add(StringHash id, float delta) {
    m_balances[id] += delta;
}

bool Inventory::canAfford(StringHash id, float cost) const {
    return get(id) >= cost;
}

bool Inventory::canAfford(std::span<const ResourceQuantity> costs) const {
    for (const auto& req : costs) {
        if (get(req.id) < req.amount) {
            return false;
        }
    }
    return true;
}

bool Inventory::tryConsume(StringHash id, float cost) {
    if (!canAfford(id, cost)) {
        return false;
    }
    add(id, -cost);
    return true;
}

bool Inventory::tryConsume(std::span<const ResourceQuantity> costs) {
    if (!canAfford(costs)) {
        return false;
    }
    for (const auto& req : costs) {
        add(req.id, -req.amount);
    }
    return true;
}

void Inventory::clear() {
    m_balances.clear();
}

void Inventory::hash(uint64_t& h) const {
    std::vector<std::pair<uint32_t, float>> sorted;
    sorted.reserve(m_balances.size());
    for (const auto& [id, amt] : m_balances) {
        sorted.emplace_back(id.value(), amt);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    for (const auto& [idVal, amt] : sorted) {
        WorldHasher::hashPod(h, idVal);
        WorldHasher::hashPod(h, amt);
    }
}

} // namespace engine::economy
