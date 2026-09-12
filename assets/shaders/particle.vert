#version 450

layout(location = 0) in vec2 inCorner;

// Per-instance attributes
layout(location = 1) in vec3 inPos;
layout(location = 2) in float inSize;
layout(location = 3) in vec4 inColor;
layout(location = 4) in float inRotation;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 cameraRight;
    vec4 cameraUp;
} pc;

void main() {
    vUv = inCorner + vec2(0.5);
    vColor = inColor;

    float c = cos(inRotation);
    float s = sin(inRotation);
    vec2 rotCorner = vec2(inCorner.x * c - inCorner.y * s, inCorner.x * s + inCorner.y * c);

    vec3 worldPos = inPos + (pc.cameraRight.xyz * rotCorner.x + pc.cameraUp.xyz * rotCorner.y) * inSize;
    gl_Position = pc.viewProj * vec4(worldPos, 1.0);
}
