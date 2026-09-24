#include "engine/ui/ComponentInspector.hpp"
#include "engine/ui/FieldAccess.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace engine::ui {
namespace {

struct PendingFieldEdit {
    bool active = false;
    FieldEdit edit;
};

void drawLabel(const FieldDesc& field) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(field.name.data(), field.name.data() + field.name.size());
    if (field.transient) {
        ImGui::SameLine();
        ImGui::TextDisabled("[transient]");
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
}

void drawIntegerWidget(void* ptr, FieldType type) {
    switch (type) {
    case FieldType::Int8:
        ImGui::InputScalar("##v", ImGuiDataType_S8, ptr);
        break;
    case FieldType::UInt8:
        ImGui::InputScalar("##v", ImGuiDataType_U8, ptr);
        break;
    case FieldType::Int16:
        ImGui::InputScalar("##v", ImGuiDataType_S16, ptr);
        break;
    case FieldType::UInt16:
        ImGui::InputScalar("##v", ImGuiDataType_U16, ptr);
        break;
    case FieldType::Int32:
        ImGui::InputScalar("##v", ImGuiDataType_S32, ptr);
        break;
    case FieldType::UInt32:
        ImGui::InputScalar("##v", ImGuiDataType_U32, ptr);
        break;
    case FieldType::Int64:
        ImGui::InputScalar("##v", ImGuiDataType_S64, ptr);
        break;
    case FieldType::UInt64:
        ImGui::InputScalar("##v", ImGuiDataType_U64, ptr);
        break;
    default:
        break;
    }
}

void drawEnumWidget(void* ptr, const FieldDesc& field) {
    switch (field.enumSize) {
    case 1:
        ImGui::InputScalar("##v", ImGuiDataType_U8, ptr);
        break;
    case 2:
        ImGui::InputScalar("##v", ImGuiDataType_U16, ptr);
        break;
    case 8:
        ImGui::InputScalar("##v", ImGuiDataType_U64, ptr);
        break;
    default:
        ImGui::InputScalar("##v", ImGuiDataType_U32, ptr);
        break;
    }
}

void drawStringWidget(void* ptr) {
    auto* value = static_cast<std::string*>(ptr);
    char buffer[256]{};
    const size_t copyLen = (std::min)(value->size(), sizeof(buffer) - 1);
    if (copyLen > 0) {
        std::memcpy(buffer, value->data(), copyLen);
    }
    if (ImGui::InputText("##v", buffer, sizeof(buffer))) {
        *value = buffer;
    }
}

void trackFieldHistory(EditorHistory* history, PendingFieldEdit& pending, const FieldEdit& beforeSnap,
                       const FieldDesc& field, void* component) {
    if (!history) {
        return;
    }
    if (ImGui::IsItemActivated()) {
        pending.active = true;
        pending.edit = beforeSnap;
    }
    if (pending.active && ImGui::IsItemDeactivatedAfterEdit()) {
        captureFieldValue(component, field, pending.edit.after, pending.edit.afterString);
        history->push(pending.edit);
        pending.active = false;
    }
}

void drawFieldWidget(void* component, const FieldDesc& field, EditorHistory* history,
                     PendingFieldEdit& pending, entt::entity entity, std::string_view componentName) {
    void* ptr = fieldPtr(component, field);
    if (!ptr) {
        return;
    }

    FieldEdit beforeSnap{};
    beforeSnap.entity = entity;
    beforeSnap.componentName.assign(componentName.begin(), componentName.end());
    beforeSnap.fieldName.assign(field.name.begin(), field.name.end());
    beforeSnap.type = field.type;
    if (history) {
        captureFieldValue(component, field, beforeSnap.before, beforeSnap.beforeString);
    }

    ImGui::PushID(field.name.data());
    drawLabel(field);

    switch (field.type) {
    case FieldType::Bool:
        ImGui::Checkbox("##v", static_cast<bool*>(ptr));
        break;
    case FieldType::Int8:
    case FieldType::UInt8:
    case FieldType::Int16:
    case FieldType::UInt16:
    case FieldType::Int32:
    case FieldType::UInt32:
    case FieldType::Int64:
    case FieldType::UInt64:
        drawIntegerWidget(ptr, field.type);
        break;
    case FieldType::Float:
        ImGui::InputFloat("##v", static_cast<float*>(ptr));
        break;
    case FieldType::Double:
        ImGui::InputDouble("##v", static_cast<double*>(ptr));
        break;
    case FieldType::Vec2:
        ImGui::InputFloat2("##v", static_cast<float*>(ptr));
        break;
    case FieldType::Vec3:
        ImGui::InputFloat3("##v", static_cast<float*>(ptr));
        break;
    case FieldType::Vec4:
        ImGui::InputFloat4("##v", static_cast<float*>(ptr));
        break;
    case FieldType::Quat:
        ImGui::InputFloat4("##v", static_cast<float*>(ptr));
        break;
    case FieldType::String:
        drawStringWidget(ptr);
        break;
    case FieldType::Entity: {
        ImGui::BeginDisabled();
        auto raw = static_cast<uint32_t>(*static_cast<entt::entity*>(ptr));
        ImGui::InputScalar("##v", ImGuiDataType_U32, &raw);
        ImGui::EndDisabled();
        ImGui::PopID();
        return;
    }
    case FieldType::Enum:
        drawEnumWidget(ptr, field);
        break;
    case FieldType::Unknown:
        ImGui::TextDisabled("(%s)", fieldTypeName(field.type));
        ImGui::PopID();
        return;
    }

    trackFieldHistory(history, pending, beforeSnap, field, component);
    ImGui::PopID();
}

void drawRegisteredComponents(entt::registry& registry, const TypeRegistry& types, entt::entity selected,
                              EditorHistory* history, PendingFieldEdit& pending) {
    bool any = false;
    for (const ComponentDesc& desc : types.components()) {
        if (!desc.hasComponent || !desc.getComponent) {
            continue;
        }
        if (!desc.hasComponent(registry, selected)) {
            continue;
        }
        void* data = desc.getComponent(registry, selected);
        if (!data) {
            continue;
        }

        any = true;
        ImGui::PushID(desc.name.data());
        if (ImGui::TreeNodeEx(desc.name.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (desc.fields.empty()) {
                ImGui::TextDisabled("No registered fields");
            }
            for (const FieldDesc& field : desc.fields) {
                drawFieldWidget(data, field, history, pending, selected, desc.name);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!any) {
        ImGui::TextDisabled("No registered components on this entity");
    }
}

void handleHistoryShortcuts(EditorHistory* history, entt::registry& registry, const TypeRegistry& types) {
    if (!history) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) {
            history->redo(registry, types);
        } else {
            history->undo(registry, types);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        history->redo(registry, types);
    }
}

} // namespace

void drawComponentInspector(entt::registry& registry, const TypeRegistry& types, entt::entity& selected,
                            EditorHistory* history) {
    if (selected != entt::null && !registry.valid(selected)) {
        selected = entt::null;
    }

    handleHistoryShortcuts(history, registry, types);

    if (!ImGui::Begin("Component Inspector")) {
        ImGui::End();
        return;
    }

    if (selected == entt::null) {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity %u", static_cast<uint32_t>(selected));
    ImGui::Separator();

    static PendingFieldEdit pending;
    drawRegisteredComponents(registry, types, selected, history, pending);
    ImGui::End();
}

} // namespace engine::ui
