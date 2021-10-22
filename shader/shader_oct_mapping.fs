#version 450 core
uniform samplerCube radiance;
uniform samplerCube normal;
uniform samplerCube radialDepth;
in vec2 texCoords;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec3 fragNormal;
layout (location = 2) out vec3 fragDist;

// Returns ±1
vec2 signNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

/** Assumes that v is a unit vector. The result is an octahedral vector on the [-1, +1] square. */
vec2 octEncode(in vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

/** Returns a unit vector. Argument o is an octahedral vector packed via octEncode,
    on the [-1, +1] square*/
vec3 octDecode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }
    return normalize(v);
}

void main()
{
    vec2 remapUv = texCoords * 2.f - vec2(1.f);
    vec3 octVec = octDecode(remapUv);
    fragColor = vec4(texture(radiance, octVec).rgb, 1.f);
    fragNormal = texture(normal, octVec).rgb;
    fragDist = texture(radialDepth, octVec).rgb;
}