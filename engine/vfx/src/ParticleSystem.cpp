#include "engine/vfx/ParticleSystem.hpp"

#include "engine/core/Log.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <glm/gtc/random.hpp>
#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>

namespace engine {

namespace {

std::vector<char> readFile(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        return {};
    }
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

struct ParticleGpuInstance {
    glm::vec3 position{0.f};
    float size{0.f};
    glm::vec4 color{1.f};
    float rotation{0.f};
};

struct ParticlePushConstants {
    glm::mat4 viewProj{1.f};
    glm::vec4 cameraRight{1.f, 0.f, 0.f, 0.f};
    glm::vec4 cameraUp{0.f, 1.f, 0.f, 0.f};
};

} // namespace

struct ParticleSystem::Impl {
    VulkanContext* ctx = nullptr;

    static constexpr size_t kMaxParticles = 10000;
    std::vector<Particle> particles;
    size_t activeCount = 0;

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    // Static Quad Buffer (Binding 0)
    VkBuffer quadBuffer = VK_NULL_HANDLE;
    VmaAllocation quadAllocation = nullptr;

    // Dynamic Instance Buffer (Binding 1)
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VmaAllocation instanceAllocation = nullptr;
    void* instanceMapped = nullptr;

    std::mt19937 rng{1337};

    float randomFloat(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }

    glm::vec3 randomUnitVector() {
        std::uniform_real_distribution<float> dist(-1.f, 1.f);
        glm::vec3 v{dist(rng), dist(rng), dist(rng)};
        while (glm::dot(v, v) < 0.001f) {
            v = glm::vec3(dist(rng), dist(rng), dist(rng));
        }
        return glm::normalize(v);
    }

    bool init(VulkanContext& inCtx) {
        ctx = &inCtx;
        particles.resize(kMaxParticles);
        activeCount = 0;

        VkDevice device = ctx->device();
        const VmaAllocator allocator = static_cast<VmaAllocator>(ctx->vma().get());

        // 1. Shaders
        const auto vertCode = readFile(SPV_PARTICLE_VERT_PATH);
        const auto fragCode = readFile(SPV_PARTICLE_FRAG_PATH);
        vertModule = createShaderModule(device, vertCode);
        fragModule = createShaderModule(device, fragCode);

        if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
            log(LogLevel::Error, "ParticleSystem: failed to load particle shaders");
            return false;
        }

        // 2. Push Constant Range & Pipeline Layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(ParticlePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            log(LogLevel::Error, "ParticleSystem: failed to create pipeline layout");
            return false;
        }

        // 3. Static Quad Vertex Buffer
        const glm::vec2 quadVertices[6] = {
            {-0.5f, -0.5f}, { 0.5f, -0.5f}, { 0.5f,  0.5f},
            {-0.5f, -0.5f}, { 0.5f,  0.5f}, {-0.5f,  0.5f}
        };

        VkBufferCreateInfo quadBufInfo{};
        quadBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        quadBufInfo.size = sizeof(quadVertices);
        quadBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        quadBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo quadAllocInfo{};
        quadAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VmaAllocationInfo quadAllocResult{};
        if (vmaCreateBuffer(allocator, &quadBufInfo, &quadAllocInfo, &quadBuffer, &quadAllocation, &quadAllocResult) != VK_SUCCESS) {
            log(LogLevel::Error, "ParticleSystem: failed to create quad buffer");
            return false;
        }
        void* mappedQuad = nullptr;
        vmaMapMemory(allocator, quadAllocation, &mappedQuad);
        std::memcpy(mappedQuad, quadVertices, sizeof(quadVertices));
        vmaUnmapMemory(allocator, quadAllocation);

        // 4. Dynamic Instance Buffer
        VkBufferCreateInfo instBufInfo{};
        instBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        instBufInfo.size = kMaxParticles * sizeof(ParticleGpuInstance);
        instBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        instBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo instAllocInfo{};
        instAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        instAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo instAllocResult{};
        if (vmaCreateBuffer(allocator, &instBufInfo, &instAllocInfo, &instanceBuffer, &instanceAllocation, &instAllocResult) != VK_SUCCESS) {
            log(LogLevel::Error, "ParticleSystem: failed to create instance buffer");
            return false;
        }
        instanceMapped = instAllocResult.pMappedData;

        // 5. Graphics Pipeline
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        // Vertex bindings: 0 = quad corner, 1 = per-instance
        std::array<VkVertexInputBindingDescription, 2> bindings{};
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(glm::vec2);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bindings[1].binding = 1;
        bindings[1].stride = sizeof(ParticleGpuInstance);
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        // Attributes
        std::array<VkVertexInputAttributeDescription, 5> attributes{};
        // Binding 0: inCorner
        attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
        // Binding 1: inPos (vec3)
        attributes[1] = {1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ParticleGpuInstance, position)};
        // Binding 1: inSize (float)
        attributes[2] = {2, 1, VK_FORMAT_R32_SFLOAT, offsetof(ParticleGpuInstance, size)};
        // Binding 1: inColor (vec4)
        attributes[3] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(ParticleGpuInstance, color)};
        // Binding 1: inRotation (float)
        attributes[4] = {4, 1, VK_FORMAT_R32_SFLOAT, offsetof(ParticleGpuInstance, rotation)};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertexInput.pVertexBindingDescriptions = bindings.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.lineWidth = 1.f;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo msaa{};
        msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth: test against scene depth, but do not write depth to allow overlapping particles
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        // Alpha Blending
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo blendState{};
        blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blendState.attachmentCount = 1;
        blendState.pAttachments = &blendAttachment;

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &msaa;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &blendState;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = ctx->renderPass();
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            log(LogLevel::Error, "ParticleSystem: failed to create particle pipeline");
            return false;
        }

        log(LogLevel::Info, "ParticleSystem initialized successfully");
        return true;
    }

    void shutdown() {
        if (!ctx) return;
        VkDevice device = ctx->device();
        const VmaAllocator allocator = static_cast<VmaAllocator>(ctx->vma().get());

        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (quadBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, quadBuffer, quadAllocation);
            quadBuffer = VK_NULL_HANDLE;
            quadAllocation = nullptr;
        }
        if (instanceBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, instanceBuffer, instanceAllocation);
            instanceBuffer = VK_NULL_HANDLE;
            instanceAllocation = nullptr;
            instanceMapped = nullptr;
        }
        if (vertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertModule, nullptr);
            vertModule = VK_NULL_HANDLE;
        }
        if (fragModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragModule, nullptr);
            fragModule = VK_NULL_HANDLE;
        }
        ctx = nullptr;
    }

    void update(float deltaTime) {
        activeCount = 0;
        for (auto& p : particles) {
            if (!p.active) continue;

            p.life += deltaTime;
            if (p.life >= p.maxLife) {
                p.active = false;
                continue;
            }

            p.velocity.y += p.gravity * deltaTime;
            const float dragFactor = std::pow(p.drag, deltaTime * 60.0f);
            p.velocity.x *= dragFactor;
            p.velocity.z *= dragFactor;
            p.position += p.velocity * deltaTime;
            p.rotation += p.rotationSpeed * deltaTime;
            activeCount++;
        }
    }

    void record(VkCommandBuffer cmd, const CameraState& camera) {
        if (!pipeline || activeCount == 0 || !instanceMapped) {
            return;
        }

        // Pack active particles into mapped instance buffer
        auto* instances = static_cast<ParticleGpuInstance*>(instanceMapped);
        uint32_t drawnInstances = 0;

        for (const auto& p : particles) {
            if (!p.active) continue;
            if (drawnInstances >= kMaxParticles) break;

            const float t = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
            instances[drawnInstances].position = p.position;
            instances[drawnInstances].size = glm::mix(p.startSize, p.endSize, t);
            instances[drawnInstances].color = glm::mix(p.startColor, p.endColor, t);
            instances[drawnInstances].rotation = p.rotation;
            drawnInstances++;
        }

        if (drawnInstances == 0) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Extract camera right & up vectors from view matrix
        const glm::vec3 camRight{camera.view[0][0], camera.view[1][0], camera.view[2][0]};
        const glm::vec3 camUp{camera.view[0][1], camera.view[1][1], camera.view[2][1]};

        ParticlePushConstants push{};
        push.viewProj = camera.viewProj;
        push.cameraRight = glm::vec4(camRight, 0.f);
        push.cameraUp = glm::vec4(camUp, 0.f);

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(ParticlePushConstants), &push);

        VkBuffer buffers[2] = {quadBuffer, instanceBuffer};
        VkDeviceSize offsets[2] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);

        vkCmdDraw(cmd, 6, drawnInstances, 0, 0);
    }

    void spawn(const glm::vec3& position, const glm::vec3& velocity,
               const glm::vec4& startColor, const glm::vec4& endColor,
               float startSize, float endSize, float lifetime,
               float gravity, float drag) {
        for (auto& p : particles) {
            if (!p.active) {
                p.position = position;
                p.velocity = velocity;
                p.startColor = startColor;
                p.endColor = endColor;
                p.startSize = startSize;
                p.endSize = endSize;
                p.life = 0.f;
                p.maxLife = lifetime;
                p.gravity = gravity;
                p.drag = drag;
                p.rotation = randomFloat(0.f, 6.28318f);
                p.rotationSpeed = randomFloat(-3.f, 3.f);
                p.active = true;
                return;
            }
        }
    }
};

ParticleSystem::ParticleSystem() : m_impl(std::make_unique<Impl>()) {}
ParticleSystem::~ParticleSystem() {
    shutdown();
}

bool ParticleSystem::init(VulkanContext& ctx) {
    if (m_initialized) return true;
    m_initialized = m_impl->init(ctx);
    return m_initialized;
}

void ParticleSystem::shutdown() {
    if (m_initialized) {
        m_impl->shutdown();
        m_initialized = false;
    }
}

void ParticleSystem::update(float deltaTime) {
    if (m_initialized) {
        m_impl->update(deltaTime);
    }
}

void ParticleSystem::record(VkCommandBuffer cmd, const CameraState& camera) {
    if (m_initialized) {
        m_impl->record(cmd, camera);
    }
}

void ParticleSystem::spawn(const glm::vec3& position, const glm::vec3& velocity,
                           const glm::vec4& startColor, const glm::vec4& endColor,
                           float startSize, float endSize, float lifetime,
                           float gravity, float drag) {
    if (m_initialized) {
        m_impl->spawn(position, velocity, startColor, endColor, startSize, endSize, lifetime, gravity, drag);
    }
}

void ParticleSystem::spawnMuzzleFlash(const glm::vec3& position, const glm::vec3& forward) {
    if (!m_initialized) return;

    // 1. Fiery fast sparks
    for (int i = 0; i < 35; ++i) {
        const glm::vec3 randOffset = m_impl->randomUnitVector() * 0.35f;
        const glm::vec3 vel = (glm::normalize(forward + randOffset)) * m_impl->randomFloat(14.f, 26.f);
        const float size = m_impl->randomFloat(0.06f, 0.14f);
        const float life = m_impl->randomFloat(0.12f, 0.30f);
        const glm::vec4 startCol{1.0f, m_impl->randomFloat(0.7f, 1.0f), 0.2f, 1.0f};
        const glm::vec4 endCol{1.0f, 0.2f, 0.05f, 0.0f};
        m_impl->spawn(position + forward * 0.2f, vel, startCol, endCol, size, 0.01f, life, -4.f, 0.94f);
    }

    // 2. Expanding smoke cloud
    for (int i = 0; i < 18; ++i) {
        const glm::vec3 randOffset = m_impl->randomUnitVector();
        const glm::vec3 vel = (forward * 0.6f + randOffset * 0.4f) * m_impl->randomFloat(1.5f, 4.0f);
        const float startSize = m_impl->randomFloat(0.12f, 0.22f);
        const float endSize = m_impl->randomFloat(0.45f, 0.85f);
        const float life = m_impl->randomFloat(0.6f, 1.2f);
        const float grey = m_impl->randomFloat(0.65f, 0.85f);
        const glm::vec4 startCol{grey, grey, grey, 0.75f};
        const glm::vec4 endCol{grey * 0.8f, grey * 0.8f, grey * 0.8f, 0.0f};
        m_impl->spawn(position, vel, startCol, endCol, startSize, endSize, life, 0.5f, 0.92f);
    }
}

void ParticleSystem::spawnImpactSparks(const glm::vec3& position, const glm::vec3& normal, float intensity) {
    if (!m_initialized) return;

    const int count = std::clamp(static_cast<int>(intensity * 30.0f), 15, 60);

    // Bouncing debris sparks
    for (int i = 0; i < count; ++i) {
        const glm::vec3 randDir = glm::normalize(normal * 0.65f + m_impl->randomUnitVector() * 0.55f);
        const float speed = m_impl->randomFloat(3.0f, 12.0f) * std::clamp(intensity, 0.5f, 2.0f);
        const float size = m_impl->randomFloat(0.04f, 0.10f);
        const float life = m_impl->randomFloat(0.25f, 0.65f);
        const glm::vec4 startCol{1.0f, m_impl->randomFloat(0.65f, 0.95f), 0.15f, 1.0f};
        const glm::vec4 endCol{0.9f, 0.15f, 0.05f, 0.0f};
        m_impl->spawn(position + normal * 0.05f, randDir * speed, startCol, endCol, size, 0.01f, life, -12.f, 0.96f);
    }

    // Impact dust puff
    for (int i = 0; i < 10; ++i) {
        const glm::vec3 dustDir = glm::normalize(normal * 0.4f + m_impl->randomUnitVector() * 0.6f);
        const float speed = m_impl->randomFloat(0.8f, 2.5f);
        const float startSize = m_impl->randomFloat(0.15f, 0.25f);
        const float endSize = m_impl->randomFloat(0.5f, 0.9f);
        const float life = m_impl->randomFloat(0.4f, 0.9f);
        const glm::vec4 startCol{0.7f, 0.65f, 0.6f, 0.6f};
        const glm::vec4 endCol{0.5f, 0.48f, 0.45f, 0.0f};
        m_impl->spawn(position + normal * 0.05f, dustDir * speed, startCol, endCol, startSize, endSize, life, 0.3f, 0.91f);
    }
}

void ParticleSystem::spawnBurst(const glm::vec3& position, int count, const glm::vec4& color, float speed) {
    if (!m_initialized) return;

    for (int i = 0; i < count; ++i) {
        const glm::vec3 vel = m_impl->randomUnitVector() * m_impl->randomFloat(speed * 0.5f, speed * 1.5f);
        const float size = m_impl->randomFloat(0.08f, 0.20f);
        const float life = m_impl->randomFloat(0.4f, 1.0f);
        glm::vec4 endColor = color;
        endColor.a = 0.0f;
        m_impl->spawn(position, vel, color, endColor, size, 0.02f, life, -5.f, 0.95f);
    }
}

size_t ParticleSystem::activeParticleCount() const {
    return m_initialized ? m_impl->activeCount : 0;
}

void ParticleSystem::clear() {
    if (m_initialized) {
        for (auto& p : m_impl->particles) {
            p.active = false;
        }
        m_impl->activeCount = 0;
    }
}

} // namespace engine
