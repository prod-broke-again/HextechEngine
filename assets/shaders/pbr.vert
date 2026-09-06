#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec3 vColor;
layout(location = 4) out vec4 vShadowCoord;

struct GpuPointLight {
    vec4 positionRadius;
    vec4 colorIntensity;
};

layout(set = 2, binding = 0) uniform LightUbo {
    mat4 lightSpaceMatrix;
    vec4 cameraPos;
    vec4 sunDir;
    vec4 lightParams;
    GpuPointLight pointLights[16];
} ubo;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
    vec4 tint;
    vec4 material;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(pc.model) * inNormal;
    vUv = inUv;
    vColor = inColor * pc.tint.rgb;
    vShadowCoord = ubo.lightSpaceMatrix * worldPos;
}
