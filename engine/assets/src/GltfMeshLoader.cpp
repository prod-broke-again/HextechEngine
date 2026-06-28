#include "engine/assets/GltfMeshLoader.hpp"

#include "engine/core/Log.hpp"

#include <cgltf.h>
#include <glm/vec4.hpp>

#include <cstring>
#include <string>

namespace engine {

namespace {

glm::vec3 readVec3(const float* data) { return {data[0], data[1], data[2]}; }

void readIndices(const cgltf_accessor* indexAcc, uint32_t baseVertex, std::vector<uint32_t>& outIndices) {
    if (!indexAcc || !indexAcc->buffer_view) {
        return;
    }

    const uint8_t* base = cgltf_buffer_view_data(indexAcc->buffer_view);
    if (!base) {
        log(LogLevel::Warn, "GltfMeshLoader: index buffer is unavailable");
        return;
    }

    cgltf_size componentSize = 0;
    switch (indexAcc->component_type) {
    case cgltf_component_type_r_8u:
        componentSize = 1;
        break;
    case cgltf_component_type_r_16u:
        componentSize = 2;
        break;
    case cgltf_component_type_r_32u:
        componentSize = 4;
        break;
    default:
        break;
    }

    if (componentSize == 0) {
        outIndices.reserve(outIndices.size() + static_cast<size_t>(indexAcc->count));
        for (cgltf_size i = 0; i < indexAcc->count; ++i) {
            outIndices.push_back(baseVertex + static_cast<uint32_t>(cgltf_accessor_read_index(indexAcc, i)));
        }
        return;
    }

    const cgltf_size stride = indexAcc->stride != 0 ? indexAcc->stride : componentSize;
    const uint8_t* indices = base + indexAcc->offset;

    outIndices.reserve(outIndices.size() + static_cast<size_t>(indexAcc->count));
    for (cgltf_size i = 0; i < indexAcc->count; ++i) {
        const uint8_t* element = indices + i * stride;
        uint32_t index = 0;
        if (componentSize == 1) {
            index = element[0];
        } else if (componentSize == 2) {
            uint16_t value = 0;
            std::memcpy(&value, element, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, element, sizeof(index));
        }
        outIndices.push_back(baseVertex + index);
    }
}

void appendPrimitive(const cgltf_primitive& primitive, GltfMeshPart& out) {
    if (primitive.type != cgltf_primitive_type_triangles) {
        return;
    }

    const cgltf_accessor* posAcc = nullptr;
    const cgltf_accessor* normAcc = nullptr;
    const cgltf_accessor* uvAcc = nullptr;
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute& attr = primitive.attributes[i];
        if (attr.type == cgltf_attribute_type_position) {
            posAcc = attr.data;
        } else if (attr.type == cgltf_attribute_type_normal) {
            normAcc = attr.data;
        } else if (attr.type == cgltf_attribute_type_texcoord && !uvAcc) {
            uvAcc = attr.data;
        }
    }
    if (!posAcc) {
        return;
    }

    if (primitive.material && primitive.material->has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr = primitive.material->pbr_metallic_roughness;
        out.baseColorFactor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                        pbr.base_color_factor[2], pbr.base_color_factor[3]);
        out.metallic = pbr.metallic_factor;
        out.roughness = pbr.roughness_factor;
        if (pbr.base_color_texture.texture) {
            out.baseColorTexture = pbr.base_color_texture.texture;
        }
    }

    const uint32_t baseVertex = static_cast<uint32_t>(out.mesh.vertices.size());
    const cgltf_size vertexCount = posAcc->count;
    out.mesh.vertices.resize(baseVertex + static_cast<size_t>(vertexCount));

    for (cgltf_size v = 0; v < vertexCount; ++v) {
        float pos[3]{};
        cgltf_accessor_read_float(posAcc, v, pos, 3);

        float norm[3]{0.f, 1.f, 0.f};
        if (normAcc) {
            cgltf_accessor_read_float(normAcc, v, norm, 3);
        }

        float uv[2]{0.f, 0.f};
        if (uvAcc) {
            cgltf_accessor_read_float(uvAcc, v, uv, 2);
        }

        MeshVertex& vertex = out.mesh.vertices[baseVertex + static_cast<size_t>(v)];
        vertex.position = readVec3(pos);
        vertex.normal = readVec3(norm);
        vertex.uv = {uv[0], uv[1]};
        vertex.color = {1.f, 1.f, 1.f};
    }

    if (primitive.indices) {
        readIndices(primitive.indices, baseVertex, out.mesh.indices);
    } else {
        for (uint32_t i = 0; i + 2 < static_cast<uint32_t>(vertexCount); i += 3) {
            out.mesh.indices.push_back(baseVertex + i);
            out.mesh.indices.push_back(baseVertex + i + 1);
            out.mesh.indices.push_back(baseVertex + i + 2);
        }
    }
}

} // namespace

std::vector<GltfMeshPart> GltfMeshLoader::extractMeshParts(const LoadedGltfCpu& gltf) {
    std::vector<GltfMeshPart> meshes;
    if (!gltf.data) {
        return meshes;
    }

    const cgltf_data* data = gltf.data.get();
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            GltfMeshPart part;
            appendPrimitive(mesh.primitives[p], part);
            if (!part.mesh.empty()) {
                meshes.push_back(std::move(part));
            }
        }
    }

    if (meshes.empty()) {
        log(LogLevel::Warn, "GltfMeshLoader: no triangle meshes found");
    }
    return meshes;
}

} // namespace engine
