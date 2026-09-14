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

MeshCpuData MeshBuilder::cylinder(float radius, float halfHeight, uint32_t segments, const glm::vec3& color) {
    MeshCpuData mesh;
    segments = std::max(6u, segments);
    const float kPi = 3.14159265358979323846f;
    const float yTop = halfHeight;
    const float yBot = -halfHeight;

    // 1. Side wall vertices
    for (uint32_t i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float theta = u * 2.f * kPi;
        const float cosT = std::cos(theta);
        const float sinT = std::sin(theta);
        const glm::vec3 normal{cosT, 0.f, sinT};

        mesh.vertices.push_back({{cosT * radius, yTop, sinT * radius}, normal, {u, 1.f}, color});
        mesh.vertices.push_back({{cosT * radius, yBot, sinT * radius}, normal, {u, 0.f}, color});
    }

    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t topL = i * 2;
        const uint32_t botL = topL + 1;
        const uint32_t topR = (i + 1) * 2;
        const uint32_t botR = topR + 1;

        mesh.indices.push_back(topL);
        mesh.indices.push_back(botL);
        mesh.indices.push_back(botR);

        mesh.indices.push_back(topL);
        mesh.indices.push_back(botR);
        mesh.indices.push_back(topR);
    }

    // 2. Top cap
    const uint32_t topCenterIdx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.f, yTop, 0.f}, {0.f, 1.f, 0.f}, {0.5f, 0.5f}, color});
    for (uint32_t i = 0; i <= segments; ++i) {
        const float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.f * kPi;
        const float cosT = std::cos(theta);
        const float sinT = std::sin(theta);
        mesh.vertices.push_back({{cosT * radius, yTop, sinT * radius}, {0.f, 1.f, 0.f}, {cosT * 0.5f + 0.5f, sinT * 0.5f + 0.5f}, color});
    }
    for (uint32_t i = 0; i < segments; ++i) {
        mesh.indices.push_back(topCenterIdx);
        mesh.indices.push_back(topCenterIdx + 1 + i);
        mesh.indices.push_back(topCenterIdx + 1 + i + 1);
    }

    // 3. Bottom cap
    const uint32_t botCenterIdx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.f, yBot, 0.f}, {0.f, -1.f, 0.f}, {0.5f, 0.5f}, color});
    for (uint32_t i = 0; i <= segments; ++i) {
        const float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.f * kPi;
        const float cosT = std::cos(theta);
        const float sinT = std::sin(theta);
        mesh.vertices.push_back({{cosT * radius, yBot, sinT * radius}, {0.f, -1.f, 0.f}, {cosT * 0.5f + 0.5f, sinT * 0.5f + 0.5f}, color});
    }
    for (uint32_t i = 0; i < segments; ++i) {
        mesh.indices.push_back(botCenterIdx);
        mesh.indices.push_back(botCenterIdx + 1 + i + 1);
        mesh.indices.push_back(botCenterIdx + 1 + i);
    }

    return mesh;
}

} // namespace engine
