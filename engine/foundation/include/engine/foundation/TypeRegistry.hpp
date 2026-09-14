#pragma once

#include "StringHash.hpp"
#include <entt/entt.hpp>
#include <string_view>

namespace engine {

class TypeRegistry {
public:
    template <typename T>
    static void registerType(std::string_view name) {
        entt::meta<T>().type(StringHash(name).value());
    }

    template <typename T>
    static auto getMetaType() {
        return entt::resolve<T>();
    }

    static auto resolve(StringHash hash) {
        return entt::resolve(hash.value());
    }
};

} // namespace engine

// Macros for convenient registration
#define ENGINE_REGISTER_TYPE(Type, Name) \
    engine::TypeRegistry::registerType<Type>(Name)
