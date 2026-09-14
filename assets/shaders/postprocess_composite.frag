#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrScene;
layout(set = 1, binding = 0) uniform sampler2D bloomTexture;

layout(push_constant) uniform Push {
    float exposure;
    float bloomIntensity;
    int toneMapper; // 0 = ACES, 1 = Khronos PBR Neutral, 2 = Reinhard, 3 = Linear
    int bloomEnabled;
} pc;

// Narkowicz ACES fit
vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Khronos PBR Neutral Tone Mapper (Color preservation without hue shifting)
vec3 pbrNeutralTonemap(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

// Reinhard tone mapper
vec3 reinhardTonemap(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 linearToSrgb(vec3 color) {
    color = max(color, vec3(0.0));
    return mix(color * 12.92, pow(color, vec3(1.0 / 2.4)) * 1.055 - 0.055, step(0.0031308, color));
}

void main() {
    vec3 hdr = texture(hdrScene, vUv).rgb;
    vec3 bloom = vec3(0.0);
    if (pc.bloomEnabled == 1) {
        bloom = texture(bloomTexture, vUv).rgb;
    }

    vec3 color = hdr + bloom * pc.bloomIntensity;
    color *= pc.exposure;

    vec3 mapped = color;
    if (pc.toneMapper == 0) {
        mapped = acesTonemap(color);
    } else if (pc.toneMapper == 1) {
        mapped = clamp(pbrNeutralTonemap(color), 0.0, 1.0);
    } else if (pc.toneMapper == 2) {
        mapped = clamp(reinhardTonemap(color), 0.0, 1.0);
    } else {
        mapped = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(linearToSrgb(mapped), 1.0);
}
