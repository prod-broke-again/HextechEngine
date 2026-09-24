#pragma once

#include "engine/foundation/TypeRegistry.hpp"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace engine::ui {

struct FieldEdit {
    entt::entity entity = entt::null;
    std::string componentName;
    std::string fieldName;
    FieldType type = FieldType::Unknown;
    std::vector<std::uint8_t> before;
    std::vector<std::uint8_t> after;
    std::string beforeString;
    std::string afterString;
};

bool captureFieldValue(const void* component, const FieldDesc& field, std::vector<std::uint8_t>& bytes,
                       std::string& text);
bool applyFieldValue(void* component, const FieldDesc& field, const std::vector<std::uint8_t>& bytes,
                     const std::string& text);

class EditorHistory {
public:
    void push(FieldEdit edit);
    bool undo(entt::registry& registry, const TypeRegistry& types);
    bool redo(entt::registry& registry, const TypeRegistry& types);
    void clear();

    [[nodiscard]] bool canUndo() const { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.empty(); }
    [[nodiscard]] std::size_t undoCount() const { return m_undo.size(); }
    [[nodiscard]] std::size_t redoCount() const { return m_redo.size(); }

private:
    bool apply(entt::registry& registry, const TypeRegistry& types, const FieldEdit& edit, bool useAfter);

    std::vector<FieldEdit> m_undo;
    std::vector<FieldEdit> m_redo;
    static constexpr std::size_t kMaxEntries = 128;
};

bool pushFieldDiff(EditorHistory& history, entt::entity entity, std::string_view componentName,
                   std::string_view fieldName, FieldType type, const std::vector<std::uint8_t>& before,
                   const std::string& beforeString, const std::vector<std::uint8_t>& after,
                   const std::string& afterString);

void commitTransformDiff(EditorHistory& history, entt::entity entity, const glm::vec3& beforeTranslation,
                         const glm::quat& beforeRotation, const glm::vec3& afterTranslation,
                         const glm::quat& afterRotation);

void commitTransformDiff(EditorHistory& history, entt::entity entity, const glm::vec3& beforeTranslation,
                         const glm::quat& beforeRotation, const glm::vec3& afterTranslation,
                         const glm::quat& afterRotation);

} // namespace engine::ui
