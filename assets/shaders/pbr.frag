#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vColor;
layout(location = 4) in vec4 vShadowCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D baseColorTex;
layout(set = 1, binding = 0) uniform sampler2D shadowMap;

struct GpuPointLight {
    vec4 positionRadius; // xyz: position, w: radius
    vec4 colorIntensity; // rgb: color, w: intensity
};

layout(set = 2, binding = 0) uniform LightUbo {
    mat4 lightSpaceMatrix;
    vec4 cameraPos;
    vec4 sunDir;
    vec4 lightParams; // x: pointLightCount
    GpuPointLight pointLights[16];
} ubo;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
    vec4 tint;
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

float calculateShadow(vec3 projCoords, float bias) {
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) {
        n = -n;
    }
    vec3 viewDir = normalize(ubo.cameraPos.xyz - vWorldPos);

    vec3 baseColor = srgbToLinear(vColor);
    if (pc.material.z > 0.5) {
        baseColor = srgbToLinear(texture(baseColorTex, vUv).rgb);
    }

    const float metallic = pc.material.x;
    const float roughness = clamp(pc.material.y, 0.04, 1.0);
    const vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    const float specPower = mix(256.0, 8.0, roughness);

    // 1. Sun direct lighting + shadow
    const vec3 sunLightDir = normalize(-ubo.sunDir.xyz);
    const float ndlSun = max(dot(n, sunLightDir), 0.0);

    vec3 projCoords = vShadowCoord.xyz / vShadowCoord.w;
    float bias = max(0.002 * (1.0 - ndlSun), 0.0004);
    float shadow = calculateShadow(projCoords, bias);

    vec3 halfDirSun = normalize(sunLightDir + viewDir);
    float specSun = pow(max(dot(n, halfDirSun), 0.0), specPower);
    vec3 sunSpecular = F0 * specSun * (1.0 - roughness) * ndlSun * shadow;
    vec3 sunDiffuse = baseColor * ndlSun * 0.85 * shadow;

    // 2. Ambient & sky hemisphere
    const vec3 fillDir = normalize(vec3(0.35, -0.25, 0.45));
    const vec3 skyColor = vec3(0.55, 0.65, 0.85);
    const vec3 groundColor = vec3(0.18, 0.16, 0.14);
    const float ndlFill = max(dot(n, fillDir), 0.0);
    const float hemi = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = baseColor * mix(groundColor, skyColor, hemi) * 0.35 + baseColor * (ndlFill * 0.15);

    // 3. Dynamic Point Lights
    vec3 pointLighting = vec3(0.0);
    int numLights = int(ubo.lightParams.x);
    for (int i = 0; i < numLights && i < 16; ++i) {
        vec3 lightPos = ubo.pointLights[i].positionRadius.xyz;
        float radius = ubo.pointLights[i].positionRadius.w;
        vec3 lightCol = ubo.pointLights[i].colorIntensity.rgb;
        float intensity = ubo.pointLights[i].colorIntensity.w;

        vec3 lightVec = lightPos - vWorldPos;
        float dist = length(lightVec);
        if (dist >= radius || dist < 0.0001) {
            continue;
        }

        vec3 L = lightVec / dist;
        float ndlL = max(dot(n, L), 0.0);
        if (ndlL <= 0.0) {
            continue;
        }

        // Windowed smooth inverse-square attenuation
        float window = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
        float atten = (window * window) / (dist * dist + 1.0);
        vec3 radiance = lightCol * (intensity * atten);

        vec3 H = normalize(L + viewDir);
        float spec = pow(max(dot(n, H), 0.0), specPower);
        vec3 specL = F0 * spec * (1.0 - roughness);
        vec3 diffL = baseColor;

        pointLighting += (diffL + specL) * radiance * ndlL;
    }

    vec3 color = ambient + sunDiffuse + sunSpecular + pointLighting;
    color = acesTonemap(color * 1.05);
    outColor = vec4(linearToSrgb(color), 1.0);
}
