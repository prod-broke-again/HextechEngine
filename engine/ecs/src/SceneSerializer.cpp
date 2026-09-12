#include "engine/ecs/SceneSerializer.hpp"

#include "engine/assets/MeshBuilder.hpp"
#include "engine/core/Log.hpp"
#include "engine/ecs/Systems.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

namespace engine {

bool SceneSerializer::serialize(const std::filesystem::path& filepath, const entt::registry& registry,
                                const glm::vec3& sunDir) {
    nlohmann::json sceneJson;
    sceneJson["version"] = 1;
    sceneJson["sunDirection"] = {sunDir.x, sunDir.y, sunDir.z};

    nlohmann::json entitiesJson = nlohmann::json::array();

    auto view = registry.view<const TransformLocal>();
    for (const auto entity : view) {
        nlohmann::json entityJson;

        // Tag
        if (const auto* tag = registry.try_get<const TagComponent>(entity)) {
            entityJson["tag"] = tag->tag;
        } else {
            entityJson["tag"] = "Entity_" + std::to_string(static_cast<uint32_t>(entity));
        }

        // TransformLocal
        const auto& trans = view.get<const TransformLocal>(entity);
        entityJson["transform"] = {
            {"translation", {trans.translation.x, trans.translation.y, trans.translation.z}},
            {"rotation", {trans.rotation.w, trans.rotation.x, trans.rotation.y, trans.rotation.z}},
            {"scale", {trans.scale.x, trans.scale.y, trans.scale.z}}
        };

        // MeshGeometryComponent (if any)
        if (const auto* geom = registry.try_get<const MeshGeometryComponent>(entity)) {
            std::string typeStr = "Custom";
            switch (geom->type) {
                case MeshGeometryType::Box: typeStr = "Box"; break;
                case MeshGeometryType::Sphere: typeStr = "Sphere"; break;
                case MeshGeometryType::Plane: typeStr = "Plane"; break;
                case MeshGeometryType::Teapot: typeStr = "Teapot"; break;
                case MeshGeometryType::Model: typeStr = "Model"; break;
                default: break;
            }
            entityJson["geometry"] = {
                {"type", typeStr},
                {"assetPath", geom->assetPath},
                {"params", {geom->params.x, geom->params.y, geom->params.z}}
            };
        }

        // MeshComponent (if any)
        if (const auto* meshComp = registry.try_get<const MeshComponent>(entity)) {
            entityJson["material"] = {
                {"tint", {meshComp->tint.r, meshComp->tint.g, meshComp->tint.b}},
                {"baseColorFactor", {meshComp->baseColorFactor.r, meshComp->baseColorFactor.g, meshComp->baseColorFactor.b, meshComp->baseColorFactor.a}},
                {"metallic", meshComp->metallic},
                {"roughness", meshComp->roughness}
            };
        }

        // ColliderComponent (if any)
        if (const auto* col = registry.try_get<const ColliderComponent>(entity)) {
            std::string shapeStr = "None";
            switch (col->shape) {
                case ColliderShapeType::Box: shapeStr = "Box"; break;
                case ColliderShapeType::Sphere: shapeStr = "Sphere"; break;
                case ColliderShapeType::ConvexHull: shapeStr = "ConvexHull"; break;
                default: break;
            }
            entityJson["collider"] = {
                {"shape", shapeStr},
                {"halfExtents", {col->halfExtents.x, col->halfExtents.y, col->halfExtents.z}},
                {"radius", col->radius},
                {"isStatic", col->isStatic},
                {"mass", col->mass}
            };
        }

        // PointLightComponent (if any)
        if (const auto* light = registry.try_get<const PointLightComponent>(entity)) {
            entityJson["pointLight"] = {
                {"color", {light->color.r, light->color.g, light->color.b}},
                {"intensity", light->intensity},
                {"radius", light->radius}
            };
        }

        // CameraComponent (if any)
        if (const auto* cam = registry.try_get<const CameraComponent>(entity)) {
            entityJson["camera"] = {
                {"fovDegrees", cam->fovDegrees},
                {"nearPlane", cam->nearPlane},
                {"farPlane", cam->farPlane},
                {"active", cam->active}
            };
        }

        entitiesJson.push_back(entityJson);
    }

    sceneJson["entities"] = entitiesJson;

    if (filepath.has_parent_path()) {
        std::filesystem::create_directories(filepath.parent_path());
    }
    std::ofstream file(filepath);
    if (!file.is_open()) {
        log(LogLevel::Error, "SceneSerializer: failed to open file for writing: " + filepath.string());
        return false;
    }

    file << sceneJson.dump(4);
    log(LogLevel::Info, "SceneSerializer: serialized " + std::to_string(entitiesJson.size()) +
                           " entities to " + filepath.string());
    return true;
}

bool SceneSerializer::deserialize(const std::filesystem::path& filepath, entt::registry& registry,
                                  glm::vec3& outSunDir, const SceneResourceContext& resCtx) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        log(LogLevel::Error, "SceneSerializer: failed to open file for reading: " + filepath.string());
        return false;
    }

    nlohmann::json sceneJson;
    try {
        file >> sceneJson;
    } catch (const std::exception& e) {
        log(LogLevel::Error, std::string("SceneSerializer: JSON parse error: ") + e.what());
        return false;
    }

    if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
        log(LogLevel::Error, "SceneSerializer: missing or invalid entities array in " + filepath.string());
        return false;
    }

    // Clear physics bodies and registry
    if (resCtx.clearPhysicsBodies) {
        resCtx.clearPhysicsBodies();
    }
    registry.clear();

    if (sceneJson.contains("sunDirection") && sceneJson["sunDirection"].is_array() &&
        sceneJson["sunDirection"].size() >= 3) {
        outSunDir.x = sceneJson["sunDirection"][0].get<float>();
        outSunDir.y = sceneJson["sunDirection"][1].get<float>();
        outSunDir.z = sceneJson["sunDirection"][2].get<float>();
    }

    size_t count = 0;
    for (const auto& entityJson : sceneJson["entities"]) {
        // If it's a Model entity
        if (entityJson.contains("geometry")) {
            const auto& geomJson = entityJson["geometry"];
            std::string geomType = geomJson.value("type", "Custom");
            if (geomType == "Model" && resCtx.spawnModel) {
                std::string path = geomJson.value("assetPath", "");
                glm::vec3 pos{0.f};
                if (entityJson.contains("transform") && entityJson["transform"].contains("translation")) {
                    const auto& t = entityJson["transform"]["translation"];
                    if (t.is_array() && t.size() >= 3) {
                        pos = {t[0].get<float>(), t[1].get<float>(), t[2].get<float>()};
                    }
                }
                float targetSize = 1.f;
                if (geomJson.contains("params") && geomJson["params"].is_array() && geomJson["params"].size() >= 1) {
                    targetSize = geomJson["params"][0].get<float>();
                }
                resCtx.spawnModel(path, pos, targetSize);
                count++;
                continue;
            }
        }

        const entt::entity entity = registry.create();
        count++;

        // Tag
        std::string tagName = entityJson.value("tag", "Entity");
        registry.emplace<TagComponent>(entity, TagComponent{tagName});

        // Transform
        TransformLocal trans{};
        if (entityJson.contains("transform")) {
            const auto& tJson = entityJson["transform"];
            if (tJson.contains("translation") && tJson["translation"].is_array() && tJson["translation"].size() >= 3) {
                trans.translation = {tJson["translation"][0].get<float>(),
                                     tJson["translation"][1].get<float>(),
                                     tJson["translation"][2].get<float>()};
            }
            if (tJson.contains("rotation") && tJson["rotation"].is_array() && tJson["rotation"].size() >= 4) {
                trans.rotation = glm::quat(tJson["rotation"][0].get<float>(),
                                           tJson["rotation"][1].get<float>(),
                                           tJson["rotation"][2].get<float>(),
                                           tJson["rotation"][3].get<float>());
            }
            if (tJson.contains("scale") && tJson["scale"].is_array() && tJson["scale"].size() >= 3) {
                trans.scale = {tJson["scale"][0].get<float>(),
                               tJson["scale"][1].get<float>(),
                               tJson["scale"][2].get<float>()};
            }
        }
        registry.emplace<TransformLocal>(entity, trans);
        registry.emplace<TransformWorld>(entity);

        // Material info if present
        MeshComponent meshComp{};
        if (entityJson.contains("material")) {
            const auto& matJson = entityJson["material"];
            if (matJson.contains("tint") && matJson["tint"].is_array() && matJson["tint"].size() >= 3) {
                meshComp.tint = {matJson["tint"][0].get<float>(),
                                 matJson["tint"][1].get<float>(),
                                 matJson["tint"][2].get<float>()};
            }
            if (matJson.contains("baseColorFactor") && matJson["baseColorFactor"].is_array() && matJson["baseColorFactor"].size() >= 4) {
                meshComp.baseColorFactor = {matJson["baseColorFactor"][0].get<float>(),
                                            matJson["baseColorFactor"][1].get<float>(),
                                            matJson["baseColorFactor"][2].get<float>(),
                                            matJson["baseColorFactor"][3].get<float>()};
            }
            meshComp.metallic = matJson.value("metallic", 0.f);
            meshComp.roughness = matJson.value("roughness", 0.5f);
        }

        // Geometry info if present
        if (entityJson.contains("geometry")) {
            const auto& geomJson = entityJson["geometry"];
            std::string geomType = geomJson.value("type", "Custom");
            std::string assetPath = geomJson.value("assetPath", "");
            glm::vec3 params{1.f};
            if (geomJson.contains("params") && geomJson["params"].is_array() && geomJson["params"].size() >= 3) {
                params = {geomJson["params"][0].get<float>(),
                          geomJson["params"][1].get<float>(),
                          geomJson["params"][2].get<float>()};
            }

            MeshGeometryComponent geomComp{};
            geomComp.assetPath = assetPath;
            geomComp.params = params;

            if (geomType == "Plane") {
                geomComp.type = MeshGeometryType::Plane;
                if (resCtx.uploadMesh) {
                    meshComp.mesh = resCtx.uploadMesh(MeshBuilder::plane(params.x, meshComp.tint));
                }
            } else if (geomType == "Box") {
                geomComp.type = MeshGeometryType::Box;
                if (resCtx.uploadMesh) {
                    meshComp.mesh = resCtx.uploadMesh(MeshBuilder::box(params, meshComp.tint));
                }
            } else if (geomType == "Sphere") {
                geomComp.type = MeshGeometryType::Sphere;
                if (resCtx.uploadMesh) {
                    meshComp.mesh = resCtx.uploadMesh(MeshBuilder::sphere(params.x, 24, 24, meshComp.tint));
                }
            } else if (geomType == "Teapot") {
                geomComp.type = MeshGeometryType::Teapot;
                if (resCtx.uploadMesh && resCtx.teapotMeshData) {
                    meshComp.mesh = resCtx.uploadMesh(*resCtx.teapotMeshData);
                }
            }

            registry.emplace<MeshGeometryComponent>(entity, geomComp);
            registry.emplace<MeshComponent>(entity, meshComp);
            registry.emplace<RenderableTag>(entity);
        }

        // Collider info if present
        if (entityJson.contains("collider")) {
            const auto& colJson = entityJson["collider"];
            std::string shape = colJson.value("shape", "None");
            bool isStatic = colJson.value("isStatic", false);
            float mass = colJson.value("mass", 1.f);
            float radius = colJson.value("radius", 0.5f);
            glm::vec3 halfExtents{0.5f};
            if (colJson.contains("halfExtents") && colJson["halfExtents"].is_array() && colJson["halfExtents"].size() >= 3) {
                halfExtents = {colJson["halfExtents"][0].get<float>(),
                               colJson["halfExtents"][1].get<float>(),
                               colJson["halfExtents"][2].get<float>()};
            }

            ColliderComponent colComp{};
            colComp.halfExtents = halfExtents;
            colComp.radius = radius;
            colComp.isStatic = isStatic;
            colComp.mass = mass;

            if (shape == "Box") {
                colComp.shape = ColliderShapeType::Box;
                if (resCtx.createBoxCollider) {
                    resCtx.createBoxCollider(entity, halfExtents, isStatic, mass);
                }
            } else if (shape == "Sphere") {
                colComp.shape = ColliderShapeType::Sphere;
                if (resCtx.createSphereCollider) {
                    resCtx.createSphereCollider(entity, radius, isStatic, mass);
                }
            } else if (shape == "ConvexHull") {
                colComp.shape = ColliderShapeType::ConvexHull;
                if (resCtx.createConvexHullCollider) {
                    resCtx.createConvexHullCollider(entity, mass);
                }
            }

            registry.emplace<ColliderComponent>(entity, colComp);
        }

        // PointLightComponent if present
        if (entityJson.contains("pointLight")) {
            const auto& lJson = entityJson["pointLight"];
            PointLightComponent plc{};
            if (lJson.contains("color") && lJson["color"].is_array() && lJson["color"].size() >= 3) {
                plc.color = {lJson["color"][0].get<float>(),
                             lJson["color"][1].get<float>(),
                             lJson["color"][2].get<float>()};
            }
            plc.intensity = lJson.value("intensity", 10.f);
            plc.radius = lJson.value("radius", 8.f);
            registry.emplace<PointLightComponent>(entity, plc);
        }

        // CameraComponent if present
        if (entityJson.contains("camera")) {
            const auto& camJson = entityJson["camera"];
            CameraComponent cam{};
            cam.fovDegrees = camJson.value("fovDegrees", 70.f);
            cam.nearPlane = camJson.value("nearPlane", 0.1f);
            cam.farPlane = camJson.value("farPlane", 500.f);
            cam.active = camJson.value("active", true);
            registry.emplace<CameraComponent>(entity, cam);
            registry.emplace<FreeFlyController>(entity);
        }
    }

    updateTransforms(registry);
    log(LogLevel::Info, "SceneSerializer: deserialized " + std::to_string(count) +
                           " entities from " + filepath.string());
    return true;
}

} // namespace engine
