#include "engine/assets/ObjMeshLoader.hpp"

#include "engine/core/Log.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include <glm/glm.hpp>

namespace engine {

namespace {

uint32_t parseFaceIndex(const std::string& token) {
    const size_t slash = token.find('/');
    const std::string indexToken = slash == std::string::npos ? token : token.substr(0, slash);
    const int index = std::stoi(indexToken);
    return static_cast<uint32_t>(index > 0 ? index - 1 : 0);
}

void triangulateFace(const std::vector<uint32_t>& face, std::vector<uint32_t>& outIndices) {
    if (face.size() < 3) {
        return;
    }
    for (size_t i = 1; i + 1 < face.size(); ++i) {
        outIndices.push_back(face[0]);
        outIndices.push_back(face[i]);
        outIndices.push_back(face[i + 1]);
    }
}

void computeNormals(MeshCpuData& mesh) {
    for (MeshVertex& vertex : mesh.vertices) {
        vertex.normal = {0.f, 0.f, 0.f};
    }

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i + 1];
        const uint32_t i2 = mesh.indices[i + 2];
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
            continue;
        }

        const glm::vec3 edge0 = mesh.vertices[i1].position - mesh.vertices[i0].position;
        const glm::vec3 edge1 = mesh.vertices[i2].position - mesh.vertices[i0].position;
        const glm::vec3 faceNormal = glm::cross(edge0, edge1);
        mesh.vertices[i0].normal += faceNormal;
        mesh.vertices[i1].normal += faceNormal;
        mesh.vertices[i2].normal += faceNormal;
    }

    for (MeshVertex& vertex : mesh.vertices) {
        const float length = glm::length(vertex.normal);
        if (length > 1e-6f) {
            vertex.normal /= length;
        } else {
            vertex.normal = {0.f, 1.f, 0.f};
        }
    }
}

} // namespace

MeshCpuData ObjMeshLoader::loadFromFile(const std::filesystem::path& path) {
    MeshCpuData mesh;
    std::ifstream file(path);
    if (!file) {
        log(LogLevel::Warn, "ObjMeshLoader: failed to open " + path.string());
        return mesh;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    positions.reserve(65536);
    colors.reserve(65536);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line.rfind("v ", 0) == 0) {
            std::istringstream stream(line.substr(2));
            glm::vec3 position{};
            glm::vec3 color{1.f};
            stream >> position.x >> position.y >> position.z;
            if (stream >> color.r >> color.g >> color.b) {
                if (color.r > 1.f || color.g > 1.f || color.b > 1.f) {
                    color /= 255.f;
                }
            }
            positions.push_back(position);
            colors.push_back(color);
            continue;
        }

        if (line.rfind("f ", 0) != 0) {
            continue;
        }

        std::istringstream stream(line.substr(2));
        std::vector<uint32_t> face;
        std::string token;
        while (stream >> token) {
            face.push_back(parseFaceIndex(token));
        }
        triangulateFace(face, mesh.indices);
    }

    if (positions.empty() || mesh.indices.empty()) {
        log(LogLevel::Warn, "ObjMeshLoader: no geometry in " + path.string());
        return mesh;
    }

    mesh.vertices.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        mesh.vertices[i].position = positions[i];
        mesh.vertices[i].normal = {0.f, 1.f, 0.f};
        mesh.vertices[i].uv = {0.f, 0.f};
        mesh.vertices[i].color = colors[i];
    }

    computeNormals(mesh);
    log(LogLevel::Info, "ObjMeshLoader: loaded " + path.filename().string() + " (" +
                           std::to_string(mesh.vertices.size()) + " verts, " +
                           std::to_string(mesh.indices.size() / 3) + " tris)");
    return mesh;
}

void ObjMeshLoader::normalize(MeshCpuData& mesh, float targetMaxExtent) {
    if (mesh.vertices.empty()) {
        return;
    }

    glm::vec3 minBounds{std::numeric_limits<float>::max()};
    glm::vec3 maxBounds{std::numeric_limits<float>::lowest()};
    for (const MeshVertex& vertex : mesh.vertices) {
        minBounds.x = std::min(minBounds.x, vertex.position.x);
        minBounds.y = std::min(minBounds.y, vertex.position.y);
        minBounds.z = std::min(minBounds.z, vertex.position.z);
        maxBounds.x = std::max(maxBounds.x, vertex.position.x);
        maxBounds.y = std::max(maxBounds.y, vertex.position.y);
        maxBounds.z = std::max(maxBounds.z, vertex.position.z);
    }

    const glm::vec3 size = maxBounds - minBounds;
    const float maxExtent = std::max({size.x, size.y, size.z, 1e-6f});
    const float scale = targetMaxExtent / maxExtent;
    const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    const glm::vec3 anchor{center.x, minBounds.y, center.z};

    for (MeshVertex& vertex : mesh.vertices) {
        vertex.position = (vertex.position - anchor) * scale;
    }
}

void ObjMeshLoader::normalize(std::vector<MeshCpuData>& meshes, float targetMaxExtent) {
    glm::vec3 minBounds{std::numeric_limits<float>::max()};
    glm::vec3 maxBounds{std::numeric_limits<float>::lowest()};
    bool hasVertices = false;

    for (const MeshCpuData& mesh : meshes) {
        for (const MeshVertex& vertex : mesh.vertices) {
            hasVertices = true;
            minBounds.x = std::min(minBounds.x, vertex.position.x);
            minBounds.y = std::min(minBounds.y, vertex.position.y);
            minBounds.z = std::min(minBounds.z, vertex.position.z);
            maxBounds.x = std::max(maxBounds.x, vertex.position.x);
            maxBounds.y = std::max(maxBounds.y, vertex.position.y);
            maxBounds.z = std::max(maxBounds.z, vertex.position.z);
        }
    }
    if (!hasVertices) {
        return;
    }

    const glm::vec3 size = maxBounds - minBounds;
    const float maxExtent = std::max({size.x, size.y, size.z, 1e-6f});
    const float scale = targetMaxExtent / maxExtent;
    const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    const glm::vec3 anchor{center.x, minBounds.y, center.z};

    for (MeshCpuData& mesh : meshes) {
        for (MeshVertex& vertex : mesh.vertices) {
            vertex.position = (vertex.position - anchor) * scale;
        }
    }
}

} // namespace engine
