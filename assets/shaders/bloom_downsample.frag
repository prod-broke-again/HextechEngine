#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform Push {
    vec4 srcResolution; // xy: 1.0 / width, 1.0 / height, zw: unused
    vec4 params;        // x: threshold, y: threshold - knee, z: 2.0 * knee, w: 0.25 / (knee + 0.00001)
    int isPrefilter;    // 1 if level 0 (prefilter + Karis average), 0 otherwise
} pc;

// Soft knee quadratic threshold
vec3 prefilter(vec3 color) {
    float brightness = max(color.r, max(color.g, color.b));
    float soft = brightness - pc.params.y;
    soft = clamp(soft, 0.0, pc.params.z);
    soft = soft * soft * pc.params.w;
    float contribution = max(soft, brightness - pc.params.x);
    contribution /= max(brightness, 0.00001);
    return color * clamp(contribution, 0.0, 1.0);
}

// Karis weighting to suppress high-frequency fireflies
float karisWeight(vec3 c) {
    return 1.0 / (1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722)));
}

void main() {
    vec2 texelSize = pc.srcResolution.xy;
    float x = texelSize.x;
    float y = texelSize.y;

    // 13 samples in 4x4 pattern with bilinear interpolation
    vec3 a = texture(srcTexture, vec2(vUv.x - 2.0 * x, vUv.y + 2.0 * y)).rgb;
    vec3 b = texture(srcTexture, vec2(vUv.x,           vUv.y + 2.0 * y)).rgb;
    vec3 c = texture(srcTexture, vec2(vUv.x + 2.0 * x, vUv.y + 2.0 * y)).rgb;

    vec3 d = texture(srcTexture, vec2(vUv.x - 2.0 * x, vUv.y)).rgb;
    vec3 e = texture(srcTexture, vec2(vUv.x,           vUv.y)).rgb;
    vec3 f = texture(srcTexture, vec2(vUv.x + 2.0 * x, vUv.y)).rgb;

    vec3 g = texture(srcTexture, vec2(vUv.x - 2.0 * x, vUv.y - 2.0 * y)).rgb;
    vec3 h = texture(srcTexture, vec2(vUv.x,           vUv.y - 2.0 * y)).rgb;
    vec3 i = texture(srcTexture, vec2(vUv.x + 2.0 * x, vUv.y - 2.0 * y)).rgb;

    vec3 j = texture(srcTexture, vec2(vUv.x - x, vUv.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(vUv.x + x, vUv.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(vUv.x - x, vUv.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(vUv.x + x, vUv.y - y)).rgb;

    if (pc.isPrefilter == 1) {
        a = prefilter(a); b = prefilter(b); c = prefilter(c);
        d = prefilter(d); e = prefilter(e); f = prefilter(f);
        g = prefilter(g); h = prefilter(h); i = prefilter(i);
        j = prefilter(j); k = prefilter(k); l = prefilter(l); m = prefilter(m);

        // 4-box Karis weighting
        vec3 box1 = (a + b + d + e) * 0.25;
        vec3 box2 = (b + c + e + f) * 0.25;
        vec3 box3 = (d + e + g + h) * 0.25;
        vec3 box4 = (e + f + h + i) * 0.25;
        vec3 box5 = (j + k + l + m) * 0.25;

        float w1 = karisWeight(box1);
        float w2 = karisWeight(box2);
        float w3 = karisWeight(box3);
        float w4 = karisWeight(box4);
        float w5 = karisWeight(box5);

        vec3 result = (box1 * w1 + box2 * w2 + box3 * w3 + box4 * w4 + box5 * w5) / (w1 + w2 + w3 + w4 + w5);
        outColor = vec4(result, 1.0);
    } else {
        vec3 result = e * 0.125;
        result += (a + c + g + i) * 0.03125;
        result += (b + d + f + h) * 0.0625;
        result += (j + k + l + m) * 0.125;
        outColor = vec4(result, 1.0);
    }
}
