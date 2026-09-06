#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUv;
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec4 vShadowCoord;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 lightSpaceMvp;
    vec4 tint;
    vec4 lightDir;
    vec4 cameraPos;
    vec4 material;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vNormal = inNormal;
    vUv = inUv;
    vColor = inColor * pc.tint.rgb;
    vShadowCoord = pc.lightSpaceMvp * vec4(inPosition, 1.0);
}
