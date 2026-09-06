#version 450

layout(location = 0) out vec3 vRayDir;

layout(push_constant) uniform Push {
    mat4 invViewProj;
    vec4 sunDir;
} pc;

void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec4 pos = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    gl_Position = pos;

    vec4 unprojected = pc.invViewProj * vec4(pos.xy, 1.0, 1.0);
    vRayDir = unprojected.xyz / unprojected.w;
}
