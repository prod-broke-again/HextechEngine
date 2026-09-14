#pragma once

#include "StringHash.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

namespace engine {

enum class FieldType : uint8_t {
    Unknown = 0,
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    String,
    Entity,
    Enum
};

template <typename T>
constexpr FieldType deduceFieldType() {
    using CleanT = std::decay_t<T>;
    if constexpr (std::is_same_v<CleanT, bool>) {
        return FieldType::Bool;
    } else if constexpr (std::is_same_v<CleanT, int8_t> || std::is_same_v<CleanT, char> || std::is_same_v<CleanT, signed char>) {
        return FieldType::Int8;
    } else if constexpr (std::is_same_v<CleanT, uint8_t> || std::is_same_v<CleanT, unsigned char>) {
        return FieldType::UInt8;
    } else if constexpr (std::is_same_v<CleanT, int16_t> || std::is_same_v<CleanT, short>) {
        return FieldType::Int16;
    } else if constexpr (std::is_same_v<CleanT, uint16_t> || std::is_same_v<CleanT, unsigned short>) {
        return FieldType::UInt16;
    } else if constexpr (std::is_same_v<CleanT, int32_t> || std::is_same_v<CleanT, int> || std::is_same_v<CleanT, long>) {
        return FieldType::Int32;
    } else if constexpr (std::is_same_v<CleanT, uint32_t> || std::is_same_v<CleanT, unsigned int> || std::is_same_v<CleanT, unsigned long>) {
        return FieldType::UInt32;
    } else if constexpr (std::is_same_v<CleanT, int64_t> || std::is_same_v<CleanT, long long>) {
        return FieldType::Int64;
    } else if constexpr (std::is_same_v<CleanT, uint64_t> || std::is_same_v<CleanT, unsigned long long>) {
        return FieldType::UInt64;
    } else if constexpr (std::is_same_v<CleanT, float>) {
        return FieldType::Float;
    } else if constexpr (std::is_same_v<CleanT, double>) {
        return FieldType::Double;
    } else if constexpr (std::is_same_v<CleanT, glm::vec2>) {
        return FieldType::Vec2;
    } else if constexpr (std::is_same_v<CleanT, glm::vec3>) {
        return FieldType::Vec3;
    } else if constexpr (std::is_same_v<CleanT, glm::vec4>) {
        return FieldType::Vec4;
    } else if constexpr (std::is_same_v<CleanT, glm::quat>) {
        return FieldType::Quat;
    } else if constexpr (std::is_same_v<CleanT, std::string>) {
        return FieldType::String;
    } else if constexpr (std::is_same_v<CleanT, entt::entity>) {
        return FieldType::Entity;
    } else if constexpr (std::is_enum_v<CleanT>) {
        return FieldType::Enum;
    } else {
        return FieldType::Unknown;
    }
}

struct FieldDesc {
    std::string_view name;
    FieldType type = FieldType::Unknown;
    size_t offset = 0;
    size_t size = 0;
    bool transient = false;
    size_t enumSize = 0;
};

struct ComponentDesc {
    std::string_view name;
    entt::id_type typeId = 0;
    uint32_t version = 1;
    size_t size = 0;
    std::vector<FieldDesc> fields;
    void (*migrate)(void* data, uint32_t fromVersion) = nullptr;

    // Component lifecycle bindings on entt::registry
    void (*emplaceDefault)(entt::registry& reg, entt::entity e) = nullptr;
    bool (*hasComponent)(const entt::registry& reg, entt::entity e) = nullptr;
    void* (*getComponent)(entt::registry& reg, entt::entity e) = nullptr;
    const void* (*getComponentConst)(const entt::registry& reg, entt::entity e) = nullptr;
    void (*removeComponent)(entt::registry& reg, entt::entity e) = nullptr;
};

template <typename T>
class ComponentBuilder {
public:
    explicit ComponentBuilder(ComponentDesc& desc) : m_desc(desc) {}

    template <typename Member>
    ComponentBuilder& field(std::string_view name, Member T::* memberPtr, bool transient = false) {
        alignas(T) char buffer[sizeof(T)];
        auto* obj = reinterpret_cast<T*>(buffer);
        size_t offset = reinterpret_cast<const char*>(&(obj->*memberPtr)) - buffer;

        FieldDesc f;
        f.name = name;
        f.type = deduceFieldType<Member>();
        f.offset = offset;
        f.size = sizeof(Member);
        f.transient = transient;
        if constexpr (std::is_enum_v<Member>) {
            f.enumSize = sizeof(std::underlying_type_t<Member>);
        }
        m_desc.fields.push_back(f);
        return *this;
    }

    template <typename Member>
    ComponentBuilder& transientField(std::string_view name, Member T::* memberPtr) {
        return field(name, memberPtr, true);
    }

private:
    ComponentDesc& m_desc;
};

class TypeRegistry {
public:
    template <typename T>
    ComponentBuilder<T> registerComponent(std::string_view name, uint32_t version = 1,
                                          void (*migrateFn)(void*, uint32_t) = nullptr) {
        const auto typeId = entt::type_hash<T>::value();
        for (auto& existing : m_components) {
            if (existing.typeId == typeId) {
                existing.name = name;
                existing.version = version;
                existing.migrate = migrateFn;
                return ComponentBuilder<T>(existing);
            }
        }

        ComponentDesc desc;
        desc.name = name;
        desc.typeId = typeId;
        desc.version = version;
        desc.size = sizeof(T);
        desc.migrate = migrateFn;

        if constexpr (std::is_default_constructible_v<T>) {
            desc.emplaceDefault = [](entt::registry& reg, entt::entity e) {
                if (!reg.all_of<T>(e)) {
                    reg.emplace<T>(e);
                }
            };
        }

        desc.hasComponent = [](const entt::registry& reg, entt::entity e) {
            return reg.all_of<T>(e);
        };

        desc.getComponent = [](entt::registry& reg, entt::entity e) -> void* {
            return reg.try_get<T>(e);
        };

        desc.getComponentConst = [](const entt::registry& reg, entt::entity e) -> const void* {
            return reg.try_get<T>(e);
        };

        desc.removeComponent = [](entt::registry& reg, entt::entity e) {
            reg.remove<T>(e);
        };

        m_components.push_back(std::move(desc));
        return ComponentBuilder<T>(m_components.back());
    }

    const ComponentDesc* findComponent(std::string_view name) const {
        for (const auto& comp : m_components) {
            if (comp.name == name) {
                return &comp;
            }
        }
        return nullptr;
    }

    const ComponentDesc* findComponent(entt::id_type typeId) const {
        for (const auto& comp : m_components) {
            if (comp.typeId == typeId) {
                return &comp;
            }
        }
        return nullptr;
    }

    template <typename T>
    const ComponentDesc* getComponentDesc() const {
        return findComponent(entt::type_hash<T>::value());
    }

    template <typename T>
    bool isRegistered() const {
        return findComponent(entt::type_hash<T>::value()) != nullptr;
    }

    bool isRegistered(entt::id_type typeId) const {
        return findComponent(typeId) != nullptr;
    }

    const std::vector<ComponentDesc>& components() const {
        return m_components;
    }

    void clear() {
        m_components.clear();
    }

    // Backward-compatibility & EnTT meta integration
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

private:
    std::vector<ComponentDesc> m_components;
};

} // namespace engine

// Convenience macros
#define ENGINE_REGISTER_TYPE(Type, Name) \
    engine::TypeRegistry::registerType<Type>(Name)

