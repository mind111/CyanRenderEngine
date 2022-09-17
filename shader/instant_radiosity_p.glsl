#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

#define pi 3.14159265359

in VSOut
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

struct VPL {
	vec4 position;
	vec4 normal;
	vec4 flux;
};

layout(std430, binding = 47) buffer VPLSSBO
{
	VPL VPLs[];
};

layout(std430, binding = 50) buffer VPLShadowHandles {
    uint64_t shadowHandles[];
};

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

uniform int numVPLs;
uniform float farClippingPlane;
uniform float indirectVisibility;
uniform vec2 outputSize;
uniform sampler2D sceneDepthBuffer; 
uniform sampler2D sceneNormalBuffer;
uniform samplerCube shadowCubemap;

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

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

void main() {
    vec3 debugColor;
    vec2 pixelCoord = gl_FragCoord.xy / outputSize;
    float pixelDepth = texture(sceneDepthBuffer, pixelCoord).r * 2.f - 1.f;
    vec3 pixelNormal = texture(sceneNormalBuffer, pixelCoord).rgb * 2.f - 1.f;
	vec3 worldSpacePosition = screenToWorld(vec3(pixelCoord * 2.f - 1.f, pixelDepth), inverse(viewSsbo.view), inverse(viewSsbo.projection));
    vec3 irradiance = vec3(0.f);
	for (int i = 0; i < numVPLs; ++i) {
        vec3 l = normalize(VPLs[i].position.xyz - worldSpacePosition);
        float d = length(VPLs[i].position.xyz - worldSpacePosition);
        // only consider contributions from upper hemisphere
        if (dot(l, pixelNormal) > 0.f) {
			if (indirectVisibility > .5f) { 
				// shadow test
				float closestDepth = texture(sampler2D(shadowHandles[i]), octEncode(-l) * .5f + .5f).r * farClippingPlane; 
				if ((d - 0.05f) < closestDepth) {
					// the geometry term
					float g = max(dot(VPLs[i].normal.xyz, -l), 0.f) * max(dot(pixelNormal, l), 0.f);
					const float radius = 0.2f;
					float atten = 2.f * (1.f - d / sqrt(d * d + radius * radius)) / (radius * radius); 
					vec3 li = VPLs[i].flux.rgb;
					irradiance += li * g * atten;
				}
			} else {
				// the geometry term
				float g = max(dot(VPLs[i].normal.xyz, -l), 0.f) * max(dot(pixelNormal, l), 0.f);
				const float radius = 1.0f;
				float atten = 2.f * (1.f - d / sqrt(d * d + radius * radius)) / (radius * radius); 
				vec3 li = VPLs[i].flux.rgb;
				irradiance += li * g * atten;
			}
		}
	}
    irradiance /= float(numVPLs);
	outColor = vec3(irradiance);
}
