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
uniform int shadowAlgorithm;
#define BASIC_SHADOW 0 
#define VSM_SHADOW 1 

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

float calculateVSMShadow(in sampler2D depthMomentsMap, in vec2 uv, in float sceneDepth) {
    float shadow = 1.f;
    float t = sceneDepth / farClippingPlane;
    vec2 depthMoments = texture(depthMomentsMap, uv).rg;
	if (t >= depthMoments.r) {
        float variance = max(depthMoments.g - depthMoments.r * depthMoments.r, 0.0001);
        shadow = variance / (variance + (sceneDepth - depthMoments.r) * (sceneDepth - depthMoments.r));
    }
    return shadow;
}

float calculateBasicShadow(in sampler2D shadowMap, in vec2 uv, in float sceneDepth) {
	float shadow = 1.f;
	float closestDepth = texture(shadowMap, uv).r * farClippingPlane; 
	if ((sceneDepth - 0.05f) > closestDepth) {
		shadow = 0.f;
	}
	return shadow;
}

void main() {
    vec3 debugColor;
    vec2 pixelCoord = gl_FragCoord.xy / outputSize;
    float pixelDepth = texture(sceneDepthBuffer, pixelCoord).r * 2.f - 1.f;
    vec3 pixelNormal = texture(sceneNormalBuffer, pixelCoord).rgb * 2.f - 1.f;
	vec3 worldSpacePosition = screenToWorld(vec3(pixelCoord * 2.f - 1.f, pixelDepth), inverse(viewSsbo.view), inverse(viewSsbo.projection));
    // virutal spherical light radius for working around shading singularity
	const float radius = 0.2f;

    vec3 irradiance = vec3(0.f);
	for (int i = 0; i < numVPLs; ++i) {
        vec3 l = normalize(VPLs[i].position.xyz - worldSpacePosition);
        float d = length(VPLs[i].position.xyz - worldSpacePosition);
		if (indirectVisibility > .5f) { 
			float shadow = 1.f;
			if (shadowAlgorithm == BASIC_SHADOW) {
				shadow = calculateBasicShadow(sampler2D(shadowHandles[i]), octEncode(-l) * .5f + .5f, d);
			} else if (shadowAlgorithm == VSM_SHADOW) {
				shadow = calculateVSMShadow(sampler2D(shadowHandles[i]), octEncode(-l) * .5f + .5f, d);
			}
			// the geometry term
			float g = max(dot(VPLs[i].normal.xyz, -l), 0.f) * max(dot(pixelNormal, l), 0.f);
			float atten = 2.f * (1.f - d / sqrt(d * d + radius * radius)) / (radius * radius); 
			vec3 li = VPLs[i].flux.rgb / float(numVPLs);
			irradiance += li * g * atten * shadow;
		} else {
			// the geometry term
			float g = max(dot(VPLs[i].normal.xyz, -l), 0.f) * max(dot(pixelNormal, l), 0.f);
			float atten = 2.f * (1.f - d / sqrt(d * d + radius * radius)) / (radius * radius); 
			vec3 li = VPLs[i].flux.rgb / float(numVPLs);
			irradiance += li * g * atten;
		}
	}
	outColor = vec3(irradiance);
}
