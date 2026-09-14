#include "engine/modules/save/SaveModule.hpp"
#include "engine/core/Log.hpp"

#include <fstream>
#include <map>
#include <unordered_map>
#include <cstring>
#include <algorithm>

namespace engine {

namespace {

constexpr uint64_t kSaveMagic = 0x3148434554584548ULL; // "HEXTECH1" in little-endian
constexpr uint32_t kEngineSaveVersion = 1;

template <typename T>
void writePod(std::ostream& out, const T& val) {
    out.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

template <typename T>
bool readPod(std::istream& in, T& val) {
    in.read(reinterpret_cast<char*>(&val), sizeof(T));
    return static_cast<bool>(in);
}

void writeString(std::ostream& out, std::string_view str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    writePod(out, len);
    if (len > 0) {
        out.write(str.data(), len);
    }
}

bool readString(std::istream& in, std::string& str) {
    uint32_t len = 0;
    if (!readPod(in, len)) return false;
    str.resize(len);
    if (len > 0) {
        in.read(&str[0], len);
    }
    return static_cast<bool>(in);
}

void writeFieldBinary(std::ostream& out, const void* fieldPtr, const FieldDesc& f) {
    switch (f.type) {
        case FieldType::Bool:
            writePod(out, *static_cast<const bool*>(fieldPtr));
            break;
        case FieldType::Int8:
            writePod(out, *static_cast<const int8_t*>(fieldPtr));
            break;
        case FieldType::UInt8:
            writePod(out, *static_cast<const uint8_t*>(fieldPtr));
            break;
        case FieldType::Int16:
            writePod(out, *static_cast<const int16_t*>(fieldPtr));
            break;
        case FieldType::UInt16:
            writePod(out, *static_cast<const uint16_t*>(fieldPtr));
            break;
        case FieldType::Int32:
            writePod(out, *static_cast<const int32_t*>(fieldPtr));
            break;
        case FieldType::UInt32:
            writePod(out, *static_cast<const uint32_t*>(fieldPtr));
            break;
        case FieldType::Int64:
            writePod(out, *static_cast<const int64_t*>(fieldPtr));
            break;
        case FieldType::UInt64:
            writePod(out, *static_cast<const uint64_t*>(fieldPtr));
            break;
        case FieldType::Float:
            writePod(out, *static_cast<const float*>(fieldPtr));
            break;
        case FieldType::Double:
            writePod(out, *static_cast<const double*>(fieldPtr));
            break;
        case FieldType::Vec2:
            writePod(out, *static_cast<const glm::vec2*>(fieldPtr));
            break;
        case FieldType::Vec3:
            writePod(out, *static_cast<const glm::vec3*>(fieldPtr));
            break;
        case FieldType::Vec4:
            writePod(out, *static_cast<const glm::vec4*>(fieldPtr));
            break;
        case FieldType::Quat:
            writePod(out, *static_cast<const glm::quat*>(fieldPtr));
            break;
        case FieldType::String:
            writeString(out, *static_cast<const std::string*>(fieldPtr));
            break;
        case FieldType::Entity: {
            uint32_t id = static_cast<uint32_t>(entt::to_integral(*static_cast<const entt::entity*>(fieldPtr)));
            writePod(out, id);
            break;
        }
        case FieldType::Enum:
        case FieldType::Unknown:
        default: {
            size_t size = f.enumSize ? f.enumSize : f.size;
            out.write(static_cast<const char*>(fieldPtr), size);
            break;
        }
    }
}

bool readFieldBinary(std::istream& in, void* fieldPtr, const FieldDesc& f,
                     const std::unordered_map<uint32_t, entt::entity>& entityMap) {
    switch (f.type) {
        case FieldType::Bool:
            return readPod(in, *static_cast<bool*>(fieldPtr));
        case FieldType::Int8:
            return readPod(in, *static_cast<int8_t*>(fieldPtr));
        case FieldType::UInt8:
            return readPod(in, *static_cast<uint8_t*>(fieldPtr));
        case FieldType::Int16:
            return readPod(in, *static_cast<int16_t*>(fieldPtr));
        case FieldType::UInt16:
            return readPod(in, *static_cast<uint16_t*>(fieldPtr));
        case FieldType::Int32:
            return readPod(in, *static_cast<int32_t*>(fieldPtr));
        case FieldType::UInt32:
            return readPod(in, *static_cast<uint32_t*>(fieldPtr));
        case FieldType::Int64:
            return readPod(in, *static_cast<int64_t*>(fieldPtr));
        case FieldType::UInt64:
            return readPod(in, *static_cast<uint64_t*>(fieldPtr));
        case FieldType::Float:
            return readPod(in, *static_cast<float*>(fieldPtr));
        case FieldType::Double:
            return readPod(in, *static_cast<double*>(fieldPtr));
        case FieldType::Vec2:
            return readPod(in, *static_cast<glm::vec2*>(fieldPtr));
        case FieldType::Vec3:
            return readPod(in, *static_cast<glm::vec3*>(fieldPtr));
        case FieldType::Vec4:
            return readPod(in, *static_cast<glm::vec4*>(fieldPtr));
        case FieldType::Quat:
            return readPod(in, *static_cast<glm::quat*>(fieldPtr));
        case FieldType::String:
            return readString(in, *static_cast<std::string*>(fieldPtr));
        case FieldType::Entity: {
            uint32_t savedId = 0;
            if (!readPod(in, savedId)) return false;
            auto it = entityMap.find(savedId);
            if (it != entityMap.end()) {
                *static_cast<entt::entity*>(fieldPtr) = it->second;
            } else {
                *static_cast<entt::entity*>(fieldPtr) = entt::null;
            }
            return true;
        }
        case FieldType::Enum:
        case FieldType::Unknown:
        default: {
            size_t size = f.enumSize ? f.enumSize : f.size;
            in.read(static_cast<char*>(fieldPtr), size);
            return static_cast<bool>(in);
        }
    }
}

void writeFieldJson(nlohmann::json& out, const void* fieldPtr, const FieldDesc& f) {
    switch (f.type) {
        case FieldType::Bool:
            out = *static_cast<const bool*>(fieldPtr);
            break;
        case FieldType::Int8:
            out = *static_cast<const int8_t*>(fieldPtr);
            break;
        case FieldType::UInt8:
            out = *static_cast<const uint8_t*>(fieldPtr);
            break;
        case FieldType::Int16:
            out = *static_cast<const int16_t*>(fieldPtr);
            break;
        case FieldType::UInt16:
            out = *static_cast<const uint16_t*>(fieldPtr);
            break;
        case FieldType::Int32:
            out = *static_cast<const int32_t*>(fieldPtr);
            break;
        case FieldType::UInt32:
            out = *static_cast<const uint32_t*>(fieldPtr);
            break;
        case FieldType::Int64:
            out = *static_cast<const int64_t*>(fieldPtr);
            break;
        case FieldType::UInt64:
            out = *static_cast<const uint64_t*>(fieldPtr);
            break;
        case FieldType::Float:
            out = *static_cast<const float*>(fieldPtr);
            break;
        case FieldType::Double:
            out = *static_cast<const double*>(fieldPtr);
            break;
        case FieldType::Vec2: {
            const auto& v = *static_cast<const glm::vec2*>(fieldPtr);
            out = {v.x, v.y};
            break;
        }
        case FieldType::Vec3: {
            const auto& v = *static_cast<const glm::vec3*>(fieldPtr);
            out = {v.x, v.y, v.z};
            break;
        }
        case FieldType::Vec4: {
            const auto& v = *static_cast<const glm::vec4*>(fieldPtr);
            out = {v.x, v.y, v.z, v.w};
            break;
        }
        case FieldType::Quat: {
            const auto& q = *static_cast<const glm::quat*>(fieldPtr);
            out = {q.w, q.x, q.y, q.z};
            break;
        }
        case FieldType::String:
            out = *static_cast<const std::string*>(fieldPtr);
            break;
        case FieldType::Entity:
            out = static_cast<uint32_t>(entt::to_integral(*static_cast<const entt::entity*>(fieldPtr)));
            break;
        case FieldType::Enum: {
            size_t size = f.enumSize ? f.enumSize : f.size;
            if (size == 1) out = static_cast<int64_t>(*static_cast<const uint8_t*>(fieldPtr));
            else if (size == 2) out = static_cast<int64_t>(*static_cast<const uint16_t*>(fieldPtr));
            else if (size == 4) out = static_cast<int64_t>(*static_cast<const uint32_t*>(fieldPtr));
            else out = *static_cast<const int64_t*>(fieldPtr);
            break;
        }
        default:
            break;
    }
}

void readFieldJson(const nlohmann::json& in, void* fieldPtr, const FieldDesc& f,
                   const std::unordered_map<uint32_t, entt::entity>& entityMap) {
    if (in.is_null()) return;

    switch (f.type) {
        case FieldType::Bool:
            *static_cast<bool*>(fieldPtr) = in.get<bool>();
            break;
        case FieldType::Int8:
            *static_cast<int8_t*>(fieldPtr) = in.get<int8_t>();
            break;
        case FieldType::UInt8:
            *static_cast<uint8_t*>(fieldPtr) = in.get<uint8_t>();
            break;
        case FieldType::Int16:
            *static_cast<int16_t*>(fieldPtr) = in.get<int16_t>();
            break;
        case FieldType::UInt16:
            *static_cast<uint16_t*>(fieldPtr) = in.get<uint16_t>();
            break;
        case FieldType::Int32:
            *static_cast<int32_t*>(fieldPtr) = in.get<int32_t>();
            break;
        case FieldType::UInt32:
            *static_cast<uint32_t*>(fieldPtr) = in.get<uint32_t>();
            break;
        case FieldType::Int64:
            *static_cast<int64_t*>(fieldPtr) = in.get<int64_t>();
            break;
        case FieldType::UInt64:
            *static_cast<uint64_t*>(fieldPtr) = in.get<uint64_t>();
            break;
        case FieldType::Float:
            *static_cast<float*>(fieldPtr) = in.get<float>();
            break;
        case FieldType::Double:
            *static_cast<double*>(fieldPtr) = in.get<double>();
            break;
        case FieldType::Vec2:
            if (in.is_array() && in.size() >= 2) {
                *static_cast<glm::vec2*>(fieldPtr) = {in[0].get<float>(), in[1].get<float>()};
            }
            break;
        case FieldType::Vec3:
            if (in.is_array() && in.size() >= 3) {
                *static_cast<glm::vec3*>(fieldPtr) = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>()};
            }
            break;
        case FieldType::Vec4:
            if (in.is_array() && in.size() >= 4) {
                *static_cast<glm::vec4*>(fieldPtr) = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>(), in[3].get<float>()};
            }
            break;
        case FieldType::Quat:
            if (in.is_array() && in.size() >= 4) {
                *static_cast<glm::quat*>(fieldPtr) = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>(), in[3].get<float>()};
            }
            break;
        case FieldType::String:
            *static_cast<std::string*>(fieldPtr) = in.get<std::string>();
            break;
        case FieldType::Entity: {
            uint32_t savedId = in.get<uint32_t>();
            auto it = entityMap.find(savedId);
            if (it != entityMap.end()) {
                *static_cast<entt::entity*>(fieldPtr) = it->second;
            } else {
                *static_cast<entt::entity*>(fieldPtr) = entt::null;
            }
            break;
        }
        case FieldType::Enum: {
            size_t size = f.enumSize ? f.enumSize : f.size;
            int64_t val = in.get<int64_t>();
            if (size == 1) *static_cast<uint8_t*>(fieldPtr) = static_cast<uint8_t>(val);
            else if (size == 2) *static_cast<uint16_t*>(fieldPtr) = static_cast<uint16_t>(val);
            else if (size == 4) *static_cast<uint32_t*>(fieldPtr) = static_cast<uint32_t>(val);
            else *static_cast<int64_t*>(fieldPtr) = val;
            break;
        }
        default:
            break;
    }
}

void checkUnregisteredComponents(const entt::registry& registry, const TypeRegistry& types) {
    for (auto [id, storage] : registry.storage()) {
        if (id == entt::type_hash<entt::entity>::value()) continue;
        if (!types.isRegistered(id)) {
            if (const auto* entityStorage = registry.storage<entt::entity>()) {
                for (auto it = entityStorage->begin(); it != entityStorage->end(); ++it) {
                    if (storage.contains(*it)) {
                        log(LogLevel::Warn, "SaveModule: entity has unregistered component in registry (typeId=" +
                                                std::to_string(id) + ")");
                        break;
                    }
                }
            }
        }
    }
}

std::vector<entt::entity> collectSortedEntities(const entt::registry& registry) {
    std::vector<entt::entity> entities;
    if (const auto* entityStorage = registry.storage<entt::entity>()) {
        for (auto it = entityStorage->begin(); it != entityStorage->end(); ++it) {
            entities.push_back(*it);
        }
    }
    std::sort(entities.begin(), entities.end(), [](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });
    return entities;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Registry Binary Serialization
// ----------------------------------------------------------------------------

bool SaveModule::saveRegistryBinary(std::ostream& out, const entt::registry& registry,
                                    const TypeRegistry& types) {
    checkUnregisteredComponents(registry, types);

    auto entities = collectSortedEntities(registry);
    uint32_t entityCount = static_cast<uint32_t>(entities.size());
    writePod(out, entityCount);

    for (entt::entity e : entities) {
        uint32_t entId = static_cast<uint32_t>(entt::to_integral(e));
        writePod(out, entId);

        std::vector<const ComponentDesc*> presentComps;
        for (const auto& compDesc : types.components()) {
            if (compDesc.hasComponent && compDesc.hasComponent(registry, e)) {
                presentComps.push_back(&compDesc);
            }
        }

        uint32_t compCount = static_cast<uint32_t>(presentComps.size());
        writePod(out, compCount);

        for (const auto* compDesc : presentComps) {
            writeString(out, compDesc->name);
            writePod(out, compDesc->version);

            const void* compPtr = compDesc->getComponentConst(registry, e);

            uint32_t fieldCount = 0;
            for (const auto& f : compDesc->fields) {
                if (!f.transient) fieldCount++;
            }
            writePod(out, fieldCount);

            for (const auto& f : compDesc->fields) {
                if (f.transient) continue;
                writeString(out, f.name);
                writePod(out, static_cast<uint8_t>(f.type));
                const void* fieldPtr = static_cast<const char*>(compPtr) + f.offset;
                writeFieldBinary(out, fieldPtr, f);
            }
        }
    }

    return static_cast<bool>(out);
}

bool SaveModule::loadRegistryBinary(std::istream& in, entt::registry& registry,
                                    const TypeRegistry& types) {
    registry.clear();

    uint32_t entityCount = 0;
    if (!readPod(in, entityCount)) return false;

    struct SavedEntityData {
        uint32_t savedId = 0;
        struct SavedCompData {
            std::string name;
            uint32_t version = 1;
            struct SavedFieldData {
                std::string name;
                uint8_t type = 0;
                std::vector<uint8_t> bytes;
                std::string stringVal;
                uint32_t entityVal = 0;
            };
            std::vector<SavedFieldData> fields;
        };
        std::vector<SavedCompData> comps;
    };

    std::vector<SavedEntityData> savedEntities(entityCount);
    std::unordered_map<uint32_t, entt::entity> entityMap;

    for (uint32_t i = 0; i < entityCount; ++i) {
        auto& entData = savedEntities[i];
        if (!readPod(in, entData.savedId)) return false;
        entt::entity newEnt = registry.create();
        entityMap[entData.savedId] = newEnt;

        uint32_t compCount = 0;
        if (!readPod(in, compCount)) return false;
        entData.comps.resize(compCount);

        for (uint32_t c = 0; c < compCount; ++c) {
            auto& compData = entData.comps[c];
            if (!readString(in, compData.name)) return false;
            if (!readPod(in, compData.version)) return false;

            uint32_t fieldCount = 0;
            if (!readPod(in, fieldCount)) return false;
            compData.fields.resize(fieldCount);

            for (uint32_t f = 0; f < fieldCount; ++f) {
                auto& fieldData = compData.fields[f];
                if (!readString(in, fieldData.name)) return false;
                if (!readPod(in, fieldData.type)) return false;

                FieldType fType = static_cast<FieldType>(fieldData.type);
                if (fType == FieldType::String) {
                    if (!readString(in, fieldData.stringVal)) return false;
                } else if (fType == FieldType::Entity) {
                    if (!readPod(in, fieldData.entityVal)) return false;
                } else {
                    size_t size = 0;
                    switch (fType) {
                        case FieldType::Bool:
                        case FieldType::Int8:
                        case FieldType::UInt8: size = 1; break;
                        case FieldType::Int16:
                        case FieldType::UInt16: size = 2; break;
                        case FieldType::Int32:
                        case FieldType::UInt32:
                        case FieldType::Float: size = 4; break;
                        case FieldType::Int64:
                        case FieldType::UInt64:
                        case FieldType::Double:
                        case FieldType::Vec2: size = 8; break;
                        case FieldType::Vec3: size = 12; break;
                        case FieldType::Vec4:
                        case FieldType::Quat: size = 16; break;
                        default: size = 4; break;
                    }
                    fieldData.bytes.resize(size);
                    in.read(reinterpret_cast<char*>(fieldData.bytes.data()), size);
                }
            }
        }
    }

    // Now apply components to created entities
    for (const auto& entData : savedEntities) {
        entt::entity e = entityMap[entData.savedId];

        for (const auto& compData : entData.comps) {
            const auto* compDesc = types.findComponent(compData.name);
            if (!compDesc) {
                log(LogLevel::Warn, "SaveModule: Unknown component '" + compData.name + "' on load; skipping.");
                continue;
            }

            if (compDesc->emplaceDefault) {
                compDesc->emplaceDefault(registry, e);
            }
            void* compPtr = compDesc->getComponent ? compDesc->getComponent(registry, e) : nullptr;
            if (!compPtr) continue;

            for (const auto& fieldData : compData.fields) {
                for (const auto& f : compDesc->fields) {
                    if (f.transient || f.name != fieldData.name) continue;

                    void* fieldPtr = static_cast<char*>(compPtr) + f.offset;
                    FieldType fType = static_cast<FieldType>(fieldData.type);

                    if (fType == FieldType::String) {
                        *static_cast<std::string*>(fieldPtr) = fieldData.stringVal;
                    } else if (fType == FieldType::Entity) {
                        auto it = entityMap.find(fieldData.entityVal);
                        *static_cast<entt::entity*>(fieldPtr) = (it != entityMap.end()) ? it->second : entt::null;
                    } else if (!fieldData.bytes.empty()) {
                        size_t copySize = std::min(fieldData.bytes.size(), f.size);
                        std::memcpy(fieldPtr, fieldData.bytes.data(), copySize);
                    }
                    break;
                }
            }

            // Version migration
            if (compData.version < compDesc->version && compDesc->migrate) {
                compDesc->migrate(compPtr, compData.version);
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// World Binary Serialization
// ----------------------------------------------------------------------------

bool SaveModule::saveBinary(std::ostream& out, const World& world) {
    writePod(out, kSaveMagic);
    writePod(out, kEngineSaveVersion);

    uint64_t tickIdx = world.tick().index;
    writePod(out, tickIdx);

    uint32_t rngCount = static_cast<uint32_t>(RngStream::_Count);
    writePod(out, rngCount);
    for (size_t s = 0; s < rngCount; ++s) {
        const auto& rng = world.rng(static_cast<RngStream>(s));
        writePod(out, rng.state());
        writePod(out, rng.inc());
    }

    return saveRegistryBinary(out, world.registry(), world.types());
}

bool SaveModule::loadBinary(std::istream& in, World& world) {
    uint64_t magic = 0;
    if (!readPod(in, magic) || magic != kSaveMagic) {
        log(LogLevel::Error, "SaveModule: invalid binary save magic");
        return false;
    }

    uint32_t version = 0;
    if (!readPod(in, version) || version > kEngineSaveVersion) {
        log(LogLevel::Error, "SaveModule: unsupported save version " + std::to_string(version));
        return false;
    }

    uint64_t tickIdx = 0;
    if (!readPod(in, tickIdx)) return false;

    uint32_t rngCount = 0;
    if (!readPod(in, rngCount)) return false;
    for (size_t s = 0; s < rngCount; ++s) {
        uint64_t state = 0;
        uint64_t inc = 0;
        if (!readPod(in, state) || !readPod(in, inc)) return false;
        if (s < static_cast<size_t>(RngStream::_Count)) {
            world.rng(static_cast<RngStream>(s)).setState(state, inc);
        }
    }

    // Set world tick
    while (world.tick().index < tickIdx) {
        world.advanceTick();
    }

    return loadRegistryBinary(in, world.registry(), world.types());
}

bool SaveModule::saveBinary(const std::filesystem::path& path, const World& world) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    return saveBinary(file, world);
}

bool SaveModule::loadBinary(const std::filesystem::path& path, World& world) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    return loadBinary(file, world);
}

// ----------------------------------------------------------------------------
// Registry JSON Serialization
// ----------------------------------------------------------------------------

bool SaveModule::saveRegistryJson(nlohmann::json& out, const entt::registry& registry,
                                  const TypeRegistry& types) {
    checkUnregisteredComponents(registry, types);

    auto entities = collectSortedEntities(registry);
    nlohmann::json entitiesArr = nlohmann::json::array();

    for (entt::entity e : entities) {
        nlohmann::json entJson;
        entJson["id"] = static_cast<uint32_t>(entt::to_integral(e));

        nlohmann::json compsJson = nlohmann::json::object();
        for (const auto& compDesc : types.components()) {
            if (compDesc.hasComponent && compDesc.hasComponent(registry, e)) {
                const void* compPtr = compDesc.getComponentConst(registry, e);
                nlohmann::json compFields;
                compFields["__version"] = compDesc.version;

                for (const auto& f : compDesc.fields) {
                    if (f.transient) continue;
                    const void* fieldPtr = static_cast<const char*>(compPtr) + f.offset;
                    nlohmann::json fieldVal;
                    writeFieldJson(fieldVal, fieldPtr, f);
                    compFields[std::string(f.name)] = fieldVal;
                }
                compsJson[std::string(compDesc.name)] = compFields;
            }
        }
        entJson["components"] = compsJson;
        entitiesArr.push_back(entJson);
    }

    out["entities"] = entitiesArr;
    return true;
}

bool SaveModule::loadRegistryJson(const nlohmann::json& in, entt::registry& registry,
                                  const TypeRegistry& types) {
    registry.clear();

    if (!in.contains("entities") || !in["entities"].is_array()) {
        return false;
    }

    const auto& entitiesArr = in["entities"];
    std::unordered_map<uint32_t, entt::entity> entityMap;

    // Pass 1: create entities
    for (const auto& entJson : entitiesArr) {
        uint32_t savedId = entJson.value("id", 0u);
        entt::entity newEnt = registry.create();
        entityMap[savedId] = newEnt;
    }

    // Pass 2: deserialize components
    for (const auto& entJson : entitiesArr) {
        uint32_t savedId = entJson.value("id", 0u);
        entt::entity e = entityMap[savedId];

        if (!entJson.contains("components") || !entJson["components"].is_object()) {
            continue;
        }

        const auto& compsObj = entJson["components"];
        for (auto it = compsObj.begin(); it != compsObj.end(); ++it) {
            std::string compName = it.key();
            const auto& compData = it.value();

            const auto* compDesc = types.findComponent(compName);
            if (!compDesc) {
                log(LogLevel::Warn, "SaveModule: Unknown component '" + compName + "' in JSON; skipping.");
                continue;
            }

            if (compDesc->emplaceDefault) {
                compDesc->emplaceDefault(registry, e);
            }
            void* compPtr = compDesc->getComponent ? compDesc->getComponent(registry, e) : nullptr;
            if (!compPtr) continue;

            uint32_t savedVersion = compData.value("__version", 1u);

            for (const auto& f : compDesc->fields) {
                if (f.transient) continue;
                std::string fName(f.name);
                if (compData.contains(fName)) {
                    void* fieldPtr = static_cast<char*>(compPtr) + f.offset;
                    readFieldJson(compData[fName], fieldPtr, f, entityMap);
                }
            }

            if (savedVersion < compDesc->version && compDesc->migrate) {
                compDesc->migrate(compPtr, savedVersion);
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// World JSON Serialization
// ----------------------------------------------------------------------------

bool SaveModule::saveJson(nlohmann::json& out, const World& world) {
    out["format"] = "HextechSave";
    out["version"] = kEngineSaveVersion;
    out["tick"] = world.tick().index;

    nlohmann::json rngArr = nlohmann::json::array();
    for (size_t s = 0; s < static_cast<size_t>(RngStream::_Count); ++s) {
        const auto& rng = world.rng(static_cast<RngStream>(s));
        rngArr.push_back({
            {"stream", s},
            {"state", rng.state()},
            {"inc", rng.inc()}
        });
    }
    out["rng"] = rngArr;

    return saveRegistryJson(out, world.registry(), world.types());
}

bool SaveModule::loadJson(const nlohmann::json& in, World& world) {
    if (!in.contains("format") || in["format"] != "HextechSave") {
        log(LogLevel::Error, "SaveModule: Invalid JSON save format");
        return false;
    }

    uint64_t tickIdx = in.value("tick", 0ULL);
    while (world.tick().index < tickIdx) {
        world.advanceTick();
    }

    if (in.contains("rng") && in["rng"].is_array()) {
        for (const auto& item : in["rng"]) {
            size_t streamIdx = item.value("stream", 0ULL);
            if (streamIdx < static_cast<size_t>(RngStream::_Count)) {
                uint64_t state = item.value("state", 0ULL);
                uint64_t inc = item.value("inc", 0ULL);
                world.rng(static_cast<RngStream>(streamIdx)).setState(state, inc);
            }
        }
    }

    return loadRegistryJson(in, world.registry(), world.types());
}

bool SaveModule::saveJson(const std::filesystem::path& path, const World& world) {
    nlohmann::json j;
    if (!saveJson(j, world)) return false;
    std::ofstream file(path);
    if (!file) return false;
    file << j.dump(2);
    return true;
}

bool SaveModule::loadJson(const std::filesystem::path& path, World& world) {
    std::ifstream file(path);
    if (!file) return false;
    nlohmann::json j;
    file >> j;
    return loadJson(j, world);
}

// ----------------------------------------------------------------------------
// Scene JSON Serialization (for SandboxApp & scenes)
// ----------------------------------------------------------------------------

bool SaveModule::saveSceneJson(const std::filesystem::path& path, const entt::registry& registry,
                              const TypeRegistry& types, const glm::vec3& sunDirection) {
    nlohmann::json root;
    root["format"] = "HextechScene";
    root["version"] = 1;
    root["sunDirection"] = {sunDirection.x, sunDirection.y, sunDirection.z};

    if (!saveRegistryJson(root, registry, types)) {
        return false;
    }

    std::ofstream file(path);
    if (!file) return false;
    file << root.dump(2);
    return true;
}

bool SaveModule::loadSceneJson(const std::filesystem::path& path, entt::registry& registry,
                              const TypeRegistry& types, glm::vec3& outSunDirection) {
    std::ifstream file(path);
    if (!file) return false;
    nlohmann::json root;
    file >> root;

    if (!root.contains("format") || root["format"] != "HextechScene") {
        // Fallback or legacy scene file check
        if (!root.contains("version") || !root.contains("entities")) {
            return false;
        }
    }

    if (root.contains("sunDirection") && root["sunDirection"].is_array() && root["sunDirection"].size() >= 3) {
        outSunDirection = {
            root["sunDirection"][0].get<float>(),
            root["sunDirection"][1].get<float>(),
            root["sunDirection"][2].get<float>()
        };
    }

    return loadRegistryJson(root, registry, types);
}

} // namespace engine
