#include "engine/ui/ComponentInspector.hpp"
#include "engine/ui/FieldAccess.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace engine::ui {
namespace {

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

void drawFieldWidget(void* component, const FieldDesc& field) {
    void* ptr = fieldPtr(component, field);
    if (!ptr) {
        return;
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
        break;
    }
    case FieldType::Enum:
        drawEnumWidget(ptr, field);
        break;
    case FieldType::Unknown:
        ImGui::TextDisabled("(%s)", fieldTypeName(field.type));
        break;
    }

    ImGui::PopID();
}

void drawRegisteredComponents(entt::registry& registry, const TypeRegistry& types, entt::entity selected) {
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
                drawFieldWidget(data, field);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!any) {
        ImGui::TextDisabled("No registered components on this entity");
    }
}

} // namespace

void drawComponentInspector(entt::registry& registry, const TypeRegistry& types, entt::entity& selected) {
    if (selected != entt::null && !registry.valid(selected)) {
        selected = entt::null;
    }

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
    drawRegisteredComponents(registry, types, selected);
    ImGui::End();
}

} // namespace engine::ui
