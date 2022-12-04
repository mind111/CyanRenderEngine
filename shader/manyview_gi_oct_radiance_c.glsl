#version 450 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D radianceAtlas;

uniform ivec2 texCoord;
uniform uint radianceRes;
uniform samplerCube radianceCubemap;

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

void main() {
	ivec2 pixelCoord = texCoord * int(radianceRes) + ivec2(gl_WorkGroupID.xy);
	vec2 uv = gl_WorkGroupID.xy / float(radianceRes);
    uv = uv * 2.f - 1.f;
    vec3 d = octDecode(uv);
    vec3 radiance = texture(radianceCubemap, d).rgb;
    imageStore(radianceAtlas, pixelCoord, vec4(radiance, 1.f));
}
