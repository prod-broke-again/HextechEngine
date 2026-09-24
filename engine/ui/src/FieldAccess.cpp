#include "engine/ui/FieldAccess.hpp"

#include <string>

namespace engine::ui {

const char* fieldTypeName(FieldType type) {
    switch (type) {
    case FieldType::Bool: return "bool";
    case FieldType::Int8: return "int8";
    case FieldType::UInt8: return "uint8";
    case FieldType::Int16: return "int16";
    case FieldType::UInt16: return "uint16";
    case FieldType::Int32: return "int32";
    case FieldType::UInt32: return "uint32";
    case FieldType::Int64: return "int64";
    case FieldType::UInt64: return "uint64";
    case FieldType::Float: return "float";
    case FieldType::Double: return "double";
    case FieldType::Vec2: return "vec2";
    case FieldType::Vec3: return "vec3";
    case FieldType::Vec4: return "vec4";
    case FieldType::Quat: return "quat";
    case FieldType::String: return "string";
    case FieldType::Entity: return "entity";
    case FieldType::Enum: return "enum";
    case FieldType::Unknown: return "unknown";
    }
    return "unknown";
}

namespace {

bool copyFieldValue(void* dst, const void* src, const FieldDesc& field) {
    if (field.type == FieldType::String) {
        *static_cast<std::string*>(dst) = *static_cast<const std::string*>(src);
        return true;
    }
    std::memcpy(dst, src, field.size);
    return true;
}

} // namespace

bool readFieldBytes(const void* component, const FieldDesc& field, void* dst, size_t dstSize) {
    const void* src = fieldPtr(component, field);
    if (!src || !dst || dstSize != field.size) {
        return false;
    }
    return copyFieldValue(dst, src, field);
}

bool writeFieldBytes(void* component, const FieldDesc& field, const void* src, size_t srcSize) {
    void* dst = fieldPtr(component, field);
    if (!dst || !src || srcSize != field.size) {
        return false;
    }
    return copyFieldValue(dst, src, field);
}

} // namespace engine::ui
