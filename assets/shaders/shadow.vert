#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec3 inColor;

layout(push_constant) uniform Push {
    mat4 lightMvp;
} pc;

void main() {
    gl_Position = pc.lightMvp * vec4(inPosition, 1.0);
}
