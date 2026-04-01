#version 450

layout(location = 0) out vec4 outColor;

void main() {
    // Placeholder: full-screen clear is done via load ops; triangle shows base PBR pass hook.
    outColor = vec4(0.15, 0.35, 0.65, 1.0);
}
