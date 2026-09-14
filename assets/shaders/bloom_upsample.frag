#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D lowerMip;
layout(set = 1, binding = 0) uniform sampler2D higherMip;

layout(push_constant) uniform Push {
    vec4 texelSize; // xy: 1.0 / lowerMip width, height; z: filter radius, w: unused
} pc;

void main() {
    float rx = pc.texelSize.x * pc.texelSize.z;
    float ry = pc.texelSize.y * pc.texelSize.z;

    // 9-tap tent filter (3x3 grid)
    vec3 a = texture(lowerMip, vec2(vUv.x - rx, vUv.y + ry)).rgb;
    vec3 b = texture(lowerMip, vec2(vUv.x,      vUv.y + ry)).rgb * 2.0;
    vec3 c = texture(lowerMip, vec2(vUv.x + rx, vUv.y + ry)).rgb;

    vec3 d = texture(lowerMip, vec2(vUv.x - rx, vUv.y)).rgb * 2.0;
    vec3 e = texture(lowerMip, vec2(vUv.x,      vUv.y)).rgb * 4.0;
    vec3 f = texture(lowerMip, vec2(vUv.x + rx, vUv.y)).rgb * 2.0;

    vec3 g = texture(lowerMip, vec2(vUv.x - rx, vUv.y - ry)).rgb;
    vec3 h = texture(lowerMip, vec2(vUv.x,      vUv.y - ry)).rgb * 2.0;
    vec3 i = texture(lowerMip, vec2(vUv.x + rx, vUv.y - ry)).rgb;

    vec3 upsampled = (a + b + c + d + e + f + g + h + i) / 16.0;
    vec3 current = texture(higherMip, vUv).rgb;

    outColor = vec4(current + upsampled, 1.0);
}
