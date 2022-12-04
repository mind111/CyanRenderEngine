#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

out vec3 outColor;

in VSOutput
{
    flat int instanceID;
	vec3 objectSpacePosition;
    flat ivec2 atlasTexCoord;
} psIn;

uniform sampler2D radianceAtlas;
uniform uint finalGatherRes;
uniform uint radianceRes;

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

/**
* p is a position on the unit cube
*/
vec3 quantizeSampleDirection(vec3 p) {
    vec3 d;
    vec3 pp = abs(p);
    if (pp.x >= pp.y && pp.x >= pp.z) {
		if (p.x > 0.f) {
            vec2 xy = vec2(-p.z, p.y);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(1.f, xy.y, -xy.x);
        }
        else {
            vec2 xy = vec2(p.z, p.y);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(-1.f, xy.y, xy.x);
        }
    }
    else if (pp.y >= pp.x && pp.y >= pp.z) {
		if (p.y > 0.f) {
            vec2 xy = vec2(p.x, -p.z);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(xy.x, 1.f, -xy.y);
        }
        else {
            vec2 xy = vec2(p.x, p.z);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(xy.x, -1.f, xy.y);
        }
    }
    else if (pp.z >= pp.x && pp.z >= pp.y) {
		if (p.z > 0.f) {
            vec2 xy = vec2(p.x, p.y);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(xy, 1.f);
        }
        else {
            vec2 xy = vec2(-p.x, p.y);
            xy = xy * .5f + .5f;
            xy = (floor(xy * finalGatherRes) + .5f) / finalGatherRes;
            xy = xy * 2.f - 1.f;
            d = vec3(-xy.x, xy.y, -1.f);
        }
    }
	return normalize(d);
}

void main() {
    // quantize sampling direction 
    vec3 pixelDir = quantizeSampleDirection(psIn.objectSpacePosition);
    vec2 localTexCoord = octEncode(pixelDir) * .5f + .5f;
    vec2 texCoord = ((vec2(psIn.atlasTexCoord) + localTexCoord) * radianceRes) / textureSize(radianceAtlas, 0).xy;
    vec3 color = texture(radianceAtlas, texCoord).rgb;

    // add color gradient to each radiance cube texel as a neat visualization
	float texelSideLen = 2.f / float(finalGatherRes);
    float colorGradient = 1.f;
    if (abs(psIn.objectSpacePosition.x) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.yz / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.yz - texelCenterCoord) / (.8 * texelSideLen);
    }
    if (abs(psIn.objectSpacePosition.y) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.xz / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.xz - texelCenterCoord) / (.8 * texelSideLen);
    }
    else if (abs(psIn.objectSpacePosition.z) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.xy / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.xy - texelCenterCoord) / (.8 * texelSideLen);
    }

	outColor = color * colorGradient;
}
