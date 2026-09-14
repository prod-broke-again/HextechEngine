#version 450

layout(location = 0) in vec3 vRayDir;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 invViewProj;
    vec4 sunDir;
} pc;

void main() {
    vec3 rayDir = normalize(vRayDir);
    vec3 sunDir = normalize(-pc.sunDir.xyz);

    float y = rayDir.y;
    vec3 zenithColor = vec3(0.18, 0.42, 0.85);
    vec3 horizonColor = vec3(0.70, 0.82, 0.95);
    vec3 groundColor = vec3(0.12, 0.12, 0.13);

    vec3 sky;
    if (y > 0.0) {
        sky = mix(horizonColor, zenithColor, pow(y, 0.55));
    } else {
        sky = mix(horizonColor, groundColor, clamp(-y * 6.0, 0.0, 1.0));
    }

    float cosAngle = dot(rayDir, sunDir);
    if (cosAngle > 0.0) {
        sky += vec3(1.0, 0.85, 0.6) * pow(cosAngle, 128.0) * 0.7;
        if (cosAngle > 0.9993) {
            sky += vec3(1.0, 0.95, 0.8) * 15.0;
        }
    }

    outColor = vec4(sky, 1.0);
}
