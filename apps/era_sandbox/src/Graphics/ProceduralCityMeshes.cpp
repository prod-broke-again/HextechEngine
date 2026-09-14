#include "Graphics/ProceduralCityMeshes.hpp"

#include <glm/geometric.hpp>
#include <cmath>
#include <vector>

namespace engine::era {

namespace {

const std::array<glm::vec2, 4> kQuadUvs{
    glm::vec2{0.f, 0.f}, glm::vec2{1.f, 0.f}, glm::vec2{1.f, 1.f}, glm::vec2{0.f, 1.f}
};

void appendQuadFace(MeshCpuData& mesh, const std::array<glm::vec3, 4>& corners,
                    const glm::vec3& normal, const glm::vec3& color) {
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (size_t i = 0; i < 4; ++i) {
        mesh.vertices.push_back({corners[i], normal, kQuadUvs[i], color});
    }
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void appendBox(MeshCpuData& mesh, const glm::vec3& center, const glm::vec3& half, const glm::vec3& color) {
    const glm::vec3 c = center;
    const glm::vec3 h = half;

    const glm::vec3 p000 = c + glm::vec3{-h.x, -h.y, -h.z};
    const glm::vec3 p001 = c + glm::vec3{-h.x, -h.y,  h.z};
    const glm::vec3 p010 = c + glm::vec3{-h.x,  h.y, -h.z};
    const glm::vec3 p011 = c + glm::vec3{-h.x,  h.y,  h.z};
    const glm::vec3 p100 = c + glm::vec3{ h.x, -h.y, -h.z};
    const glm::vec3 p101 = c + glm::vec3{ h.x, -h.y,  h.z};
    const glm::vec3 p110 = c + glm::vec3{ h.x,  h.y, -h.z};
    const glm::vec3 p111 = c + glm::vec3{ h.x,  h.y,  h.z};

    appendQuadFace(mesh, {p100, p110, p111, p101}, { 1.f,  0.f,  0.f}, color);
    appendQuadFace(mesh, {p001, p011, p010, p000}, {-1.f,  0.f,  0.f}, color);
    appendQuadFace(mesh, {p010, p011, p111, p110}, { 0.f,  1.f,  0.f}, color);
    appendQuadFace(mesh, {p001, p000, p100, p101}, { 0.f, -1.f,  0.f}, color);
    appendQuadFace(mesh, {p101, p111, p011, p001}, { 0.f,  0.f,  1.f}, color);
    appendQuadFace(mesh, {p100, p000, p010, p110}, { 0.f,  0.f, -1.f}, color);
}

void appendPitchedRoof(MeshCpuData& mesh, const glm::vec3& center, const glm::vec3& halfBase,
                       float peakHeight, const glm::vec3& color) {
    const glm::vec3 c = center;
    const glm::vec3 h = halfBase;

    const glm::vec3 b0 = c + glm::vec3{-h.x, 0.f, -h.z};
    const glm::vec3 b1 = c + glm::vec3{ h.x, 0.f, -h.z};
    const glm::vec3 b2 = c + glm::vec3{ h.x, 0.f,  h.z};
    const glm::vec3 b3 = c + glm::vec3{-h.x, 0.f,  h.z};

    const glm::vec3 ridge0 = c + glm::vec3{0.f, peakHeight, -h.z * 0.95f};
    const glm::vec3 ridge1 = c + glm::vec3{0.f, peakHeight,  h.z * 0.95f};

    // Left slope
    const glm::vec3 nLeft = glm::normalize(glm::vec3(-peakHeight, h.x, 0.f));
    appendQuadFace(mesh, {b0, ridge0, ridge1, b3}, nLeft, color);

    // Right slope
    const glm::vec3 nRight = glm::normalize(glm::vec3(peakHeight, h.x, 0.f));
    appendQuadFace(mesh, {b2, ridge1, ridge0, b1}, nRight, color);

    // Front gable
    const uint32_t baseF = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({b3, {0.f, 0.f, 1.f}, {0.f, 0.f}, color * 0.9f});
    mesh.vertices.push_back({b2, {0.f, 0.f, 1.f}, {1.f, 0.f}, color * 0.9f});
    mesh.vertices.push_back({ridge1, {0.f, 0.f, 1.f}, {0.5f, 1.f}, color * 0.9f});
    mesh.indices.insert(mesh.indices.end(), {baseF, baseF + 1, baseF + 2});

    // Back gable
    const uint32_t baseB = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({b1, {0.f, 0.f, -1.f}, {0.f, 0.f}, color * 0.9f});
    mesh.vertices.push_back({b0, {0.f, 0.f, -1.f}, {1.f, 0.f}, color * 0.9f});
    mesh.vertices.push_back({ridge0, {0.f, 0.f, -1.f}, {0.5f, 1.f}, color * 0.9f});
    mesh.indices.insert(mesh.indices.end(), {baseB, baseB + 1, baseB + 2});
}

void appendPineTree(MeshCpuData& mesh, const glm::vec3& pos, float scale = 1.0f) {
    // Brown trunk
    appendBox(mesh, pos + glm::vec3(0.0f, 0.12f * scale, 0.0f),
              {0.035f * scale, 0.12f * scale, 0.035f * scale}, {0.35f, 0.22f, 0.12f});
    // Layered green pine foliage (3 tiers of needle pyramids)
    appendBox(mesh, pos + glm::vec3(0.0f, 0.26f * scale, 0.0f),
              {0.14f * scale, 0.06f * scale, 0.14f * scale}, {0.18f, 0.42f, 0.16f});
    appendBox(mesh, pos + glm::vec3(0.0f, 0.36f * scale, 0.0f),
              {0.10f * scale, 0.06f * scale, 0.10f * scale}, {0.22f, 0.48f, 0.18f});
    appendBox(mesh, pos + glm::vec3(0.0f, 0.45f * scale, 0.0f),
              {0.06f * scale, 0.05f * scale, 0.06f * scale}, {0.28f, 0.55f, 0.22f});
}

// ----------------------------------------------------------------------------
// 1. Stone Age Building Meshes
// ----------------------------------------------------------------------------

MeshCpuData buildCampfireMesh() {
    MeshCpuData mesh;
    // Circular stone ring
    constexpr int kStones = 8;
    for (int i = 0; i < kStones; ++i) {
        const float angle = static_cast<float>(i) * 6.283185f / static_cast<float>(kStones);
        const glm::vec3 pos{std::cos(angle) * 0.35f, 0.05f, std::sin(angle) * 0.35f};
        appendBox(mesh, pos, {0.07f, 0.05f, 0.07f}, {0.55f, 0.55f, 0.58f});
    }

    // Crossed firewood logs
    appendBox(mesh, {0.0f, 0.08f, 0.0f}, {0.25f, 0.04f, 0.06f}, {0.42f, 0.26f, 0.14f});
    appendBox(mesh, {0.0f, 0.10f, 0.0f}, {0.06f, 0.04f, 0.25f}, {0.38f, 0.22f, 0.12f});

    // Glowing core ember
    appendBox(mesh, {0.0f, 0.14f, 0.0f}, {0.08f, 0.06f, 0.08f}, {1.0f, 0.55f, 0.1f});
    return mesh;
}

MeshCpuData buildResidenceStoneAgeMesh() {
    MeshCpuData mesh;
    // Timber walls
    appendBox(mesh, {0.0f, 0.22f, 0.0f}, {0.35f, 0.22f, 0.35f}, {0.62f, 0.44f, 0.26f});

    // Thatched pitched roof
    appendPitchedRoof(mesh, {0.0f, 0.44f, 0.0f}, {0.38f, 0.0f, 0.38f}, 0.32f, {0.78f, 0.68f, 0.32f});

    // Wooden door
    appendBox(mesh, {0.0f, 0.15f, 0.36f}, {0.10f, 0.15f, 0.02f}, {0.30f, 0.18f, 0.10f});
    return mesh;
}

// ----------------------------------------------------------------------------
// 2. Bronze Age Building Meshes (Evolution Upgrades)
// ----------------------------------------------------------------------------

MeshCpuData buildTownCenterBronzeAgeMesh() {
    MeshCpuData mesh;
    // Sturdy stone foundation
    appendBox(mesh, {0.0f, 0.08f, 0.0f}, {0.44f, 0.08f, 0.44f}, {0.60f, 0.60f, 0.64f});

    // Main hall walls
    appendBox(mesh, {0.0f, 0.28f, 0.0f}, {0.36f, 0.20f, 0.36f}, {0.72f, 0.58f, 0.42f});

    // 4 Corner timber columns
    for (float sx : {-0.36f, 0.36f}) {
        for (float sz : {-0.36f, 0.36f}) {
            appendBox(mesh, {sx, 0.28f, sz}, {0.05f, 0.20f, 0.05f}, {0.42f, 0.26f, 0.14f});
        }
    }

    // Grand pitched shingled roof
    appendPitchedRoof(mesh, {0.0f, 0.48f, 0.0f}, {0.42f, 0.0f, 0.42f}, 0.40f, {0.78f, 0.34f, 0.18f});

    // Chieftain banner pole with bronze heraldry
    appendBox(mesh, {0.0f, 0.72f, 0.0f}, {0.03f, 0.24f, 0.03f}, {0.45f, 0.30f, 0.15f});
    appendBox(mesh, {0.08f, 0.86f, 0.0f}, {0.08f, 0.06f, 0.02f}, {0.95f, 0.78f, 0.20f});
    return mesh;
}

MeshCpuData buildResidenceBronzeAgeMesh() {
    MeshCpuData mesh;
    // Stone foundation base
    appendBox(mesh, {0.0f, 0.06f, 0.0f}, {0.38f, 0.06f, 0.38f}, {0.58f, 0.58f, 0.62f});

    // Plastered clay & timber walls
    appendBox(mesh, {0.0f, 0.26f, 0.0f}, {0.35f, 0.20f, 0.35f}, {0.82f, 0.76f, 0.64f});

    // Terracotta tiled roof
    appendPitchedRoof(mesh, {0.0f, 0.46f, 0.0f}, {0.39f, 0.0f, 0.39f}, 0.32f, {0.75f, 0.30f, 0.18f});

    // Stone chimney on side
    appendBox(mesh, {0.24f, 0.45f, -0.15f}, {0.07f, 0.25f, 0.07f}, {0.52f, 0.52f, 0.56f});

    // Wooden arched door
    appendBox(mesh, {0.0f, 0.18f, 0.36f}, {0.10f, 0.16f, 0.02f}, {0.40f, 0.24f, 0.12f});
    return mesh;
}

// ----------------------------------------------------------------------------
// 3. Shared Production & Infrastructure Meshes
// ----------------------------------------------------------------------------

MeshCpuData buildLumberjackMesh() {
    MeshCpuData mesh;
    // 1. Main log cabin (warm timber)
    appendBox(mesh, {-0.12f, 0.22f, -0.05f}, {0.26f, 0.22f, 0.28f}, {0.52f, 0.34f, 0.18f});

    // Timber corner posts (darker wood)
    appendBox(mesh, {-0.37f, 0.22f, -0.32f}, {0.035f, 0.23f, 0.035f}, {0.35f, 0.22f, 0.11f});
    appendBox(mesh, { 0.13f, 0.22f, -0.32f}, {0.035f, 0.23f, 0.035f}, {0.35f, 0.22f, 0.11f});
    appendBox(mesh, {-0.37f, 0.22f,  0.22f}, {0.035f, 0.23f, 0.035f}, {0.35f, 0.22f, 0.11f});
    appendBox(mesh, { 0.13f, 0.22f,  0.22f}, {0.035f, 0.23f, 0.035f}, {0.35f, 0.22f, 0.11f});

    // Pitched timber roof
    appendPitchedRoof(mesh, {-0.12f, 0.44f, -0.05f}, {0.29f, 0.0f, 0.31f}, 0.26f, {0.38f, 0.24f, 0.12f});

    // Small stone chimney
    appendBox(mesh, {-0.28f, 0.52f, -0.18f}, {0.06f, 0.18f, 0.06f}, {0.58f, 0.58f, 0.60f});

    // 2. Large outdoor log storage pile (bright cut log rings)
    appendBox(mesh, {0.28f, 0.08f, -0.12f}, {0.14f, 0.08f, 0.24f}, {0.72f, 0.48f, 0.22f});
    appendBox(mesh, {0.28f, 0.18f, -0.12f}, {0.10f, 0.07f, 0.20f}, {0.65f, 0.42f, 0.20f});
    appendBox(mesh, {0.28f, 0.27f, -0.12f}, {0.06f, 0.06f, 0.16f}, {0.58f, 0.38f, 0.18f});

    // Front cut ends of the logs (bright golden wood cross-sections)
    appendBox(mesh, {0.28f, 0.08f, 0.125f}, {0.135f, 0.075f, 0.01f}, {0.84f, 0.68f, 0.38f});
    appendBox(mesh, {0.28f, 0.18f, 0.085f}, {0.095f, 0.065f, 0.01f}, {0.84f, 0.68f, 0.38f});

    // 3. Woodcutter chopping block & axe
    appendBox(mesh, {0.26f, 0.12f, 0.26f}, {0.09f, 0.12f, 0.09f}, {0.48f, 0.32f, 0.16f});
    appendBox(mesh, {0.26f, 0.245f, 0.26f}, {0.085f, 0.01f, 0.085f}, {0.82f, 0.65f, 0.36f}); // Stump top ring
    // Axe handle (wood) & head (iron)
    appendBox(mesh, {0.26f, 0.32f, 0.26f}, {0.015f, 0.08f, 0.015f}, {0.60f, 0.40f, 0.22f});
    appendBox(mesh, {0.26f, 0.36f, 0.28f}, {0.02f, 0.035f, 0.04f}, {0.78f, 0.80f, 0.85f});

    // 4. Wooden door
    appendBox(mesh, {-0.12f, 0.16f, 0.235f}, {0.08f, 0.16f, 0.01f}, {0.32f, 0.18f, 0.08f});

    // 5. Pine trees (stands of forest timber beside the woodcutter)
    appendPineTree(mesh, {-0.28f, 0.0f,  0.30f}, 1.15f);
    appendPineTree(mesh, { 0.32f, 0.0f, -0.32f}, 0.90f);

    return mesh;
}

MeshCpuData buildFisheryMesh() {
    MeshCpuData mesh;
    // Wooden stilt base
    appendBox(mesh, {0.0f, 0.06f, 0.0f}, {0.38f, 0.06f, 0.38f}, {0.35f, 0.25f, 0.18f});

    // Coastal hut
    appendBox(mesh, {-0.10f, 0.24f, -0.05f}, {0.24f, 0.18f, 0.28f}, {0.32f, 0.46f, 0.52f});
    appendPitchedRoof(mesh, {-0.10f, 0.42f, -0.05f}, {0.26f, 0.0f, 0.30f}, 0.22f, {0.68f, 0.58f, 0.38f});

    // Fishing pier walkway
    appendBox(mesh, {0.24f, 0.07f, 0.15f}, {0.12f, 0.04f, 0.24f}, {0.45f, 0.32f, 0.20f});

    // Drying rack with fish
    appendBox(mesh, {0.26f, 0.20f, -0.20f}, {0.03f, 0.12f, 0.15f}, {0.25f, 0.18f, 0.10f});
    return mesh;
}

MeshCpuData buildStoneQuarryMesh() {
    MeshCpuData mesh;
    // Tiered stone bedrock quarry
    appendBox(mesh, {-0.10f, 0.12f, -0.10f}, {0.35f, 0.12f, 0.32f}, {0.55f, 0.55f, 0.58f});
    appendBox(mesh, { 0.15f, 0.22f,  0.10f}, {0.25f, 0.10f, 0.28f}, {0.48f, 0.48f, 0.52f});

    // Quarried stone blocks
    appendBox(mesh, { 0.18f, 0.07f, -0.22f}, {0.10f, 0.07f, 0.10f}, {0.70f, 0.70f, 0.75f});
    appendBox(mesh, {-0.24f, 0.06f,  0.22f}, {0.08f, 0.06f, 0.10f}, {0.65f, 0.65f, 0.70f});
    return mesh;
}

MeshCpuData buildWheatFarmMesh() {
    MeshCpuData mesh;
    // Farmhouse shack
    appendBox(mesh, {-0.20f, 0.18f, -0.15f}, {0.18f, 0.18f, 0.22f}, {0.70f, 0.55f, 0.35f});
    appendPitchedRoof(mesh, {-0.20f, 0.36f, -0.15f}, {0.20f, 0.0f, 0.24f}, 0.20f, {0.75f, 0.35f, 0.20f});

    // Golden wheat field rows
    for (int r = -1; r <= 1; ++r) {
        const float z = static_cast<float>(r) * 0.18f + 0.10f;
        appendBox(mesh, {0.18f, 0.08f, z}, {0.22f, 0.08f, 0.06f}, {0.88f, 0.78f, 0.22f});
    }
    return mesh;
}

MeshCpuData buildBakeryMesh() {
    MeshCpuData mesh;
    // Bakery building
    appendBox(mesh, {-0.05f, 0.22f, 0.0f}, {0.32f, 0.22f, 0.32f}, {0.72f, 0.60f, 0.50f});
    appendPitchedRoof(mesh, {-0.05f, 0.44f, 0.0f}, {0.34f, 0.0f, 0.34f}, 0.26f, {0.75f, 0.32f, 0.20f});

    // Stone oven chimney
    appendBox(mesh, {0.22f, 0.35f, -0.18f}, {0.08f, 0.35f, 0.08f}, {0.45f, 0.40f, 0.42f});

    // Flour sacks
    appendBox(mesh, {0.22f, 0.08f, 0.18f}, {0.08f, 0.08f, 0.08f}, {0.88f, 0.85f, 0.78f});
    return mesh;
}

MeshCpuData buildRoadMesh() {
    MeshCpuData mesh;
    // Packed trail slab
    appendBox(mesh, {0.0f, 0.015f, 0.0f}, {0.48f, 0.012f, 0.48f}, {0.50f, 0.40f, 0.26f});
    return mesh;
}

// ----------------------------------------------------------------------------
// 4. Courier / Worker Unit Meshes
// ----------------------------------------------------------------------------

MeshCpuData buildCarrierStoneAgeMesh() {
    MeshCpuData mesh;
    // Legs
    appendBox(mesh, {-0.04f, 0.08f, 0.0f}, {0.03f, 0.08f, 0.03f}, {0.55f, 0.40f, 0.28f});
    appendBox(mesh, { 0.04f, 0.08f, 0.0f}, {0.03f, 0.08f, 0.03f}, {0.55f, 0.40f, 0.28f});

    // Body / Leather hide tunic
    appendBox(mesh, {0.0f, 0.22f, 0.0f}, {0.08f, 0.08f, 0.06f}, {0.62f, 0.45f, 0.25f});

    // Head
    appendBox(mesh, {0.0f, 0.34f, 0.0f}, {0.05f, 0.05f, 0.05f}, {0.85f, 0.68f, 0.52f});

    // Large resource sack on back
    appendBox(mesh, {0.0f, 0.22f, -0.10f}, {0.09f, 0.10f, 0.08f}, {0.75f, 0.58f, 0.35f});
    return mesh;
}

MeshCpuData buildCarrierBronzeAgeMesh() {
    MeshCpuData mesh;
    // Legs & body
    appendBox(mesh, {-0.04f, 0.08f, -0.08f}, {0.03f, 0.08f, 0.03f}, {0.35f, 0.42f, 0.55f});
    appendBox(mesh, { 0.04f, 0.08f, -0.08f}, {0.03f, 0.08f, 0.03f}, {0.35f, 0.42f, 0.55f});
    appendBox(mesh, {0.0f, 0.22f, -0.08f}, {0.07f, 0.08f, 0.06f}, {0.40f, 0.55f, 0.70f});
    appendBox(mesh, {0.0f, 0.34f, -0.08f}, {0.05f, 0.05f, 0.05f}, {0.85f, 0.68f, 0.52f});

    // Wooden Wheelbarrow / Cart in front
    appendBox(mesh, {0.0f, 0.12f, 0.14f}, {0.11f, 0.06f, 0.12f}, {0.50f, 0.32f, 0.18f});

    // Side wheels
    appendBox(mesh, {-0.12f, 0.08f, 0.14f}, {0.02f, 0.08f, 0.08f}, {0.32f, 0.20f, 0.10f});
    appendBox(mesh, { 0.12f, 0.08f, 0.14f}, {0.02f, 0.08f, 0.08f}, {0.32f, 0.20f, 0.10f});

    // Wheelbarrow handles connecting to courier hands
    appendBox(mesh, {-0.07f, 0.18f, 0.02f}, {0.02f, 0.02f, 0.08f}, {0.45f, 0.28f, 0.15f});
    appendBox(mesh, { 0.07f, 0.18f, 0.02f}, {0.02f, 0.02f, 0.08f}, {0.45f, 0.28f, 0.15f});

    // Carried cargo crate
    appendBox(mesh, {0.0f, 0.21f, 0.14f}, {0.08f, 0.05f, 0.09f}, {0.85f, 0.75f, 0.35f});
    return mesh;
}

MeshCpuData buildAlertIconMesh() {
    MeshCpuData mesh;
    const glm::vec3 alertColor{1.0f, 0.84f, 0.08f}; // Bright amber-yellow
    const glm::vec3 coreColor{1.0f, 0.95f, 0.40f};  // Glowing core

    // Bottom dot of exclamation mark
    appendBox(mesh, {0.0f, 0.05f, 0.0f}, {0.04f, 0.04f, 0.04f}, alertColor);
    appendBox(mesh, {0.0f, 0.05f, 0.0f}, {0.025f, 0.025f, 0.045f}, coreColor);

    // Upper exclamation bar (tapered top)
    appendBox(mesh, {0.0f, 0.22f, 0.0f}, {0.045f, 0.10f, 0.045f}, alertColor);
    appendBox(mesh, {0.0f, 0.23f, 0.0f}, {0.03f, 0.08f, 0.05f}, coreColor);
    appendBox(mesh, {0.0f, 0.30f, 0.0f}, {0.055f, 0.035f, 0.055f}, alertColor);
    return mesh;
}

} // namespace

void ProceduralCityMeshes::init(GpuMeshCache& cache) {
    m_buildingMeshes.clear();
    m_carrierMeshes.fill(engine::kInvalidGpuMesh);

    m_alertIconMesh = cache.upload(buildAlertIconMesh());

    // 1. Shared meshes
    const uint32_t lumberjackMesh = cache.upload(buildLumberjackMesh());
    const uint32_t fisheryMesh = cache.upload(buildFisheryMesh());
    const uint32_t quarryMesh = cache.upload(buildStoneQuarryMesh());
    const uint32_t farmMesh = cache.upload(buildWheatFarmMesh());
    const uint32_t bakeryMesh = cache.upload(buildBakeryMesh());
    const uint32_t roadMesh = cache.upload(buildRoadMesh());

    // 2. Register building meshes by StringHash
    m_buildingMeshes[BuildingIds::TownCenter] = {cache.upload(buildCampfireMesh()), cache.upload(buildTownCenterBronzeAgeMesh())};
    m_buildingMeshes[BuildingIds::Residence] = {cache.upload(buildResidenceStoneAgeMesh()), cache.upload(buildResidenceBronzeAgeMesh())};
    m_buildingMeshes[BuildingIds::Lumberjack] = {lumberjackMesh, lumberjackMesh};
    m_buildingMeshes[BuildingIds::Fishery] = {fisheryMesh, fisheryMesh};
    m_buildingMeshes[BuildingIds::StoneQuarry] = {quarryMesh, quarryMesh};
    m_buildingMeshes[BuildingIds::WheatFarm] = {farmMesh, farmMesh};
    m_buildingMeshes[BuildingIds::Bakery] = {bakeryMesh, bakeryMesh};
    m_buildingMeshes[BuildingIds::Road] = {roadMesh, roadMesh};

    // 3. Courier / Worker unit meshes
    m_carrierMeshes[static_cast<size_t>(EraType::StoneAge)] = cache.upload(buildCarrierStoneAgeMesh());
    m_carrierMeshes[static_cast<size_t>(EraType::BronzeAge)] = cache.upload(buildCarrierBronzeAgeMesh());
}

uint32_t ProceduralCityMeshes::getBuildingMesh(StringHash type, EraType era) const {
    const size_t eraIdx = static_cast<size_t>(era);
    if (eraIdx >= kEraCount) return engine::kInvalidGpuMesh;

    auto it = m_buildingMeshes.find(type);
    if (it != m_buildingMeshes.end()) {
        return it->second[eraIdx];
    }

    // Check if BuildingDef has a custom mesh identifier
    const auto& def = getBuildingDef(type);
    if (!def.mesh.empty()) {
        auto meshIt = m_buildingMeshes.find(StringHash(def.mesh));
        if (meshIt != m_buildingMeshes.end()) {
            return meshIt->second[eraIdx];
        }
    }

    // Default fallback mesh for unknown buildings
    auto fallbackIt = m_buildingMeshes.find(BuildingIds::Lumberjack);
    if (fallbackIt != m_buildingMeshes.end()) {
        return fallbackIt->second[eraIdx];
    }

    return engine::kInvalidGpuMesh;
}

uint32_t ProceduralCityMeshes::getCarrierMesh(EraType era) const {
    const size_t eraIdx = static_cast<size_t>(era);
    if (eraIdx < kEraCount) {
        return m_carrierMeshes[eraIdx];
    }
    return engine::kInvalidGpuMesh;
}

} // namespace engine::era
