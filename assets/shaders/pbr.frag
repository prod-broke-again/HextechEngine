#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUv;
layout(location = 2) in vec3 vColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D baseColorTex;

layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 tint;
    vec4 lightDir;
    vec4 cameraPos;
    vec4 material;
} pc;

vec3 srgbToLinear(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(0.04045, color));
}

vec3 linearToSrgb(vec3 color) {
    color = max(color, vec3(0.0));
    return mix(color * 12.92, pow(color, vec3(1.0 / 2.4)) * 1.055 - 0.055, step(0.0031308, color));
}

vec3 acesTonemap(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 viewDir = normalize(pc.cameraPos.xyz - vec3(0.0)); // approximate view for specular

    vec3 baseColor = srgbToLinear(vColor);
    if (pc.material.z > 0.5) {
        baseColor = srgbToLinear(texture(baseColorTex, vUv).rgb);
    }

    const float metallic = pc.material.x;
    const float roughness = clamp(pc.material.y, 0.04, 1.0);

    const vec3 lightDir = normalize(-pc.lightDir.xyz);
    const vec3 fillDir = normalize(vec3(0.35, -0.25, 0.45));
    const vec3 skyColor = vec3(0.55, 0.65, 0.85);
    const vec3 groundColor = vec3(0.18, 0.16, 0.14);

    const float ndl = max(dot(n, lightDir), 0.0);
    const float ndlFill = max(dot(n, fillDir), 0.0);
    const float hemi = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = baseColor * mix(groundColor, skyColor, hemi) * 0.35;

    vec3 diffuse = baseColor * (ndl * 0.85 + ndlFill * 0.25);

    vec3 halfDir = normalize(lightDir + viewDir);
    float specPower = mix(256.0, 8.0, roughness);
    float spec = pow(max(dot(n, halfDir), 0.0), specPower);
    vec3 specColor = mix(vec3(0.04), baseColor, metallic);
    vec3 specular = specColor * spec * (1.0 - roughness) * ndl;

    vec3 color = ambient + diffuse + specular;
    color = acesTonemap(color * 1.05);
    outColor = vec4(linearToSrgb(color), 1.0);
}
