#include "engine/ui/EditorHistory.hpp"
#include "engine/ui/FieldAccess.hpp"

#include <cstring>

namespace engine::ui {
namespace {

const FieldDesc* findField(const ComponentDesc& desc, std::string_view name) {
    for (const FieldDesc& field : desc.fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
}

} // namespace

bool captureFieldValue(const void* component, const FieldDesc& field, std::vector<std::uint8_t>& bytes,
                       std::string& text) {
    if (field.type == FieldType::String) {
        return readField(component, field, text);
    }
    if (field.type == FieldType::Unknown || field.size == 0) {
        return false;
    }
    bytes.resize(field.size);
    return readFieldBytes(component, field, bytes.data(), bytes.size());
}

bool applyFieldValue(void* component, const FieldDesc& field, const std::vector<std::uint8_t>& bytes,
                     const std::string& text) {
    if (field.type == FieldType::String) {
        return writeField(component, field, text);
    }
    if (bytes.size() != field.size) {
        return false;
    }
    return writeFieldBytes(component, field, bytes.data(), bytes.size());
}

void EditorHistory::push(FieldEdit edit) {
    if (edit.entity == entt::null || edit.componentName.empty() || edit.fieldName.empty()) {
        return;
    }
    m_undo.push_back(std::move(edit));
    if (m_undo.size() > kMaxEntries) {
        m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
}

bool EditorHistory::apply(entt::registry& registry, const TypeRegistry& types, const FieldEdit& edit,
                          bool useAfter) {
    if (!registry.valid(edit.entity)) {
        return false;
    }
    const ComponentDesc* desc = types.findComponent(edit.componentName);
    if (!desc || !desc->hasComponent || !desc->getComponent) {
        return false;
    }
    if (!desc->hasComponent(registry, edit.entity)) {
        return false;
    }
    void* data = desc->getComponent(registry, edit.entity);
    const FieldDesc* field = findField(*desc, edit.fieldName);
    if (!data || !field) {
        return false;
    }
    if (useAfter) {
        return applyFieldValue(data, *field, edit.after, edit.afterString);
    }
    return applyFieldValue(data, *field, edit.before, edit.beforeString);
}

bool EditorHistory::undo(entt::registry& registry, const TypeRegistry& types) {
    if (m_undo.empty()) {
        return false;
    }
    FieldEdit edit = std::move(m_undo.back());
    m_undo.pop_back();
    if (!apply(registry, types, edit, false)) {
        return false;
    }
    m_redo.push_back(std::move(edit));
    return true;
}

bool EditorHistory::redo(entt::registry& registry, const TypeRegistry& types) {
    if (m_redo.empty()) {
        return false;
    }
    FieldEdit edit = std::move(m_redo.back());
    m_redo.pop_back();
    if (!apply(registry, types, edit, true)) {
        return false;
    }
    m_undo.push_back(std::move(edit));
    return true;
}

void EditorHistory::clear() {
    m_undo.clear();
    m_redo.clear();
}

bool pushFieldDiff(EditorHistory& history, entt::entity entity, std::string_view componentName,
                   std::string_view fieldName, FieldType type, const std::vector<std::uint8_t>& before,
                   const std::string& beforeString, const std::vector<std::uint8_t>& after,
                   const std::string& afterString) {
    if (before == after && beforeString == afterString) {
        return false;
    }
    FieldEdit edit;
    edit.entity = entity;
    edit.componentName.assign(componentName.begin(), componentName.end());
    edit.fieldName.assign(fieldName.begin(), fieldName.end());
    edit.type = type;
    edit.before = before;
    edit.after = after;
    edit.beforeString = beforeString;
    edit.afterString = afterString;
    history.push(std::move(edit));
    return true;
}

namespace {

std::vector<std::uint8_t> bytesOf(const void* src, std::size_t size) {
    std::vector<std::uint8_t> out(size);
    std::memcpy(out.data(), src, size);
    return out;
}

} // namespace

void commitTransformDiff(EditorHistory& history, entt::entity entity, const glm::vec3& beforeTranslation,
                         const glm::quat& beforeRotation, const glm::vec3& afterTranslation,
                         const glm::quat& afterRotation) {
    pushFieldDiff(history, entity, "TransformLocal", "translation", FieldType::Vec3,
                  bytesOf(&beforeTranslation, sizeof(beforeTranslation)), {},
                  bytesOf(&afterTranslation, sizeof(afterTranslation)), {});
    pushFieldDiff(history, entity, "TransformLocal", "rotation", FieldType::Quat,
                  bytesOf(&beforeRotation, sizeof(beforeRotation)), {},
                  bytesOf(&afterRotation, sizeof(afterRotation)), {});
}

} // namespace engine::ui
