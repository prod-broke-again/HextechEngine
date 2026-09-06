#include "engine/assets/MeshBuilder.hpp"

#include <algorithm>
#include <array>
#include <cmath>

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

MeshCpuData MeshBuilder::sphere(float radius, uint32_t rings, uint32_t sectors,
                                const glm::vec3& color) {
    MeshCpuData mesh;
    rings = std::max(rings, 3u);
    sectors = std::max(sectors, 3u);

    constexpr float kPi = 3.14159265358979323846f;
    const float R = 1.f / static_cast<float>(rings);
    const float S = 1.f / static_cast<float>(sectors);

    mesh.vertices.reserve((rings + 1) * (sectors + 1));
    mesh.indices.reserve(rings * sectors * 6);

    for (uint32_t r = 0; r <= rings; ++r) {
        const float phi = static_cast<float>(r) * kPi * R;
        const float y = std::cos(phi);
        const float sinPhi = std::sin(phi);

        for (uint32_t s = 0; s <= sectors; ++s) {
            const float theta = static_cast<float>(s) * 2.f * kPi * S;
            const float x = sinPhi * std::cos(theta);
            const float z = sinPhi * std::sin(theta);

            const glm::vec3 normal{x, y, z};
            const glm::vec3 pos = normal * radius;
            const glm::vec2 uv{static_cast<float>(s) * S, static_cast<float>(r) * R};

            mesh.vertices.push_back({pos, normal, uv, color});
        }
    }

    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < sectors; ++s) {
            const uint32_t i0 = r * (sectors + 1) + s;
            const uint32_t i1 = (r + 1) * (sectors + 1) + s;
            const uint32_t i2 = (r + 1) * (sectors + 1) + (s + 1);
            const uint32_t i3 = r * (sectors + 1) + (s + 1);

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    return mesh;
}

} // namespace engine
