#version 450

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = vUv - vec2(0.5);
    float distSq = dot(d, d);
    if (distSq > 0.25) {
        discard;
    }
    // Smooth circular radial fade
    float alpha = smoothstep(0.25, 0.0, distSq);
    outColor = vec4(vColor.rgb, vColor.a * alpha);
}
