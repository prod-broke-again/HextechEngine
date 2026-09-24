#pragma once

#include "engine/foundation/TypeRegistry.hpp"

#include <cstddef>

namespace engine::ui {

[[nodiscard]] inline const void* fieldPtr(const void* component, const FieldDesc& field) {
    if (!component) {
        return nullptr;
    }
    return static_cast<const char*>(component) + field.offset;
}

[[nodiscard]] inline void* fieldPtr(void* component, const FieldDesc& field) {
    if (!component) {
        return nullptr;
    }
    return static_cast<char*>(component) + field.offset;
}

[[nodiscard]] inline bool isIntegerFieldType(FieldType type) {
    switch (type) {
    case FieldType::Int8:
    case FieldType::UInt8:
    case FieldType::Int16:
    case FieldType::UInt16:
    case FieldType::Int32:
    case FieldType::UInt32:
    case FieldType::Int64:
    case FieldType::UInt64:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] inline bool fieldTypeCompatible(const FieldDesc& field, FieldType actual, size_t size) {
    if (size != field.size) {
        return false;
    }
    if (field.type == FieldType::Enum) {
        return actual == FieldType::Enum || isIntegerFieldType(actual);
    }
    return field.type == actual;
}

[[nodiscard]] const char* fieldTypeName(FieldType type);

bool readFieldBytes(const void* component, const FieldDesc& field, void* dst, size_t dstSize);
bool writeFieldBytes(void* component, const FieldDesc& field, const void* src, size_t srcSize);

template <typename T>
bool readField(const void* component, const FieldDesc& field, T& out) {
    const FieldType actual = deduceFieldType<T>();
    if (!fieldTypeCompatible(field, actual, sizeof(T))) {
        return false;
    }
    return readFieldBytes(component, field, &out, sizeof(T));
}

template <typename T>
bool writeField(void* component, const FieldDesc& field, const T& value) {
    const FieldType actual = deduceFieldType<T>();
    if (!fieldTypeCompatible(field, actual, sizeof(T))) {
        return false;
    }
    return writeFieldBytes(component, field, &value, sizeof(T));
}

} // namespace engine::ui
