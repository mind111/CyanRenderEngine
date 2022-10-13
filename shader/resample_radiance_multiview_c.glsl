#version 450 core
layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D radianceAtlas;

uniform samplerCube hemicubeMultiView;

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

layout(std430, binding = 56) buffer HemicubeIndexBuffer
{
    ivec2 hemicubeIndices[];
};

void main() {
	int hemicubeId = int(gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x);
	vec2 hemicubeFaceSize = vec2(1.f) / gl_NumWorkGroups.xy;
	vec2 outputTexCoord = vec2(gl_LocalInvocationID.xy) / vec2(gl_WorkGroupSize.xy) * 2.f - 1.f;

    vec3 d = octDecode(outputTexCoord);
    // convert d to an according direction in this hemicube block
    vec3 dd = abs(d);
    if (dd.x > dd.y && dd.x > dd.z) {
        d *= 1.f / dd.x;
        vec3 radiance = vec3(0.f);
        if (d.x > 0.f) {
            vec3 u = vec3(0.f, 0.f, -1.f);
            vec3 v = vec3(0.f, 1.f,  0.f);
            vec3 origin = vec3(1.f, -1.f, 1.f);
            vec2 localCoord = hemicubeFaceSize * (vec2(-d.z, d.y) * .5f + .5f);
            vec2 uv = hemicubeFaceSize * gl_WorkGroupID.xy + localCoord;
            vec3 sampleDir = origin + uv.x * u + uv.y * v;
            radiance = texture(hemicubeMultiView, sampleDir).rgb;
        } else {
            vec3 u = vec3(0.f, 0.f,  1.f);
            vec3 v = vec3(0.f, 1.f,  0.f);
            vec3 origin = vec3(-1.f, -1.f, -1.f);
            vec2 localCoord = hemicubeFaceSize * (vec2(d.z, d.y) * .5f + .5f);
            vec2 uv = hemicubeFaceSize * gl_WorkGroupID.xy + localCoord;
            vec3 sampleDir = origin + uv.x * u + uv.y * v;
            radiance = texture(hemicubeMultiView, sampleDir).rgb;
        }
		ivec2 outputCoord = hemicubeIndices[hemicubeId] * ivec2(gl_WorkGroupSize.xy) + ivec2(gl_LocalInvocationID.xy);
		imageStore(radianceAtlas, outputCoord, vec4(radiance, 1.f));
    }
    if (dd.y > dd.x && dd.y > dd.z) {
        if (d.y > 0.f) {
        } else {
        }
    }
    else if (dd.z > dd.y && dd.z > dd.x) {
		if (d.z > 0.f) {
		} else {
        }
    }
}

