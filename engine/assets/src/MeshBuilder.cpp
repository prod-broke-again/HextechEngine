#include "engine/assets/MeshBuilder.hpp"

#include <array>

namespace engine {

namespace {

const std::array<glm::vec2, 4> kUnitUvs{
    glm::vec2{0.f, 0.f}, glm::vec2{1.f, 0.f}, glm::vec2{1.f, 1.f}, glm::vec2{0.f, 1.f}};

void appendBoxFace(MeshCpuData& mesh, const std::array<glm::vec3, 4>& corners, const glm::vec3& normal,
                   const glm::vec3& color) {
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (size_t i = 0; i < corners.size(); ++i) {
        mesh.vertices.push_back({corners[i], normal, kUnitUvs[i], color});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
}

void buildBox(MeshCpuData& mesh, const glm::vec3& half, const glm::vec3& color) {
    const glm::vec3 p000{-half.x, -half.y, -half.z};
    const glm::vec3 p001{-half.x, -half.y, half.z};
    const glm::vec3 p010{-half.x, half.y, -half.z};
    const glm::vec3 p011{-half.x, half.y, half.z};
    const glm::vec3 p100{half.x, -half.y, -half.z};
    const glm::vec3 p101{half.x, -half.y, half.z};
    const glm::vec3 p110{half.x, half.y, -half.z};
    const glm::vec3 p111{half.x, half.y, half.z};

    appendBoxFace(mesh, {p100, p110, p111, p101}, {1.f, 0.f, 0.f}, color);
    appendBoxFace(mesh, {p001, p011, p010, p000}, {-1.f, 0.f, 0.f}, color);
    appendBoxFace(mesh, {p010, p011, p111, p110}, {0.f, 1.f, 0.f}, color);
    appendBoxFace(mesh, {p001, p000, p100, p101}, {0.f, -1.f, 0.f}, color);
    appendBoxFace(mesh, {p101, p111, p011, p001}, {0.f, 0.f, 1.f}, color);
    appendBoxFace(mesh, {p100, p000, p010, p110}, {0.f, 0.f, -1.f}, color);
}

} // namespace

MeshCpuData MeshBuilder::plane(float halfExtent, const glm::vec3& color) {
    MeshCpuData mesh;
    const float h = halfExtent;
    mesh.vertices = {
        {{-h, 0.f, -h}, {0.f, 1.f, 0.f}, glm::vec2{0.f, 0.f}, color},
        {{h, 0.f, -h}, {0.f, 1.f, 0.f}, glm::vec2{1.f, 0.f}, color},
        {{h, 0.f, h}, {0.f, 1.f, 0.f}, glm::vec2{1.f, 1.f}, color},
        {{-h, 0.f, h}, {0.f, 1.f, 0.f}, glm::vec2{0.f, 1.f}, color},
    };
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

MeshCpuData MeshBuilder::box(const glm::vec3& halfExtents, const glm::vec3& color) {
    MeshCpuData mesh;
    buildBox(mesh, halfExtents, color);
    return mesh;
}

} // namespace engine
