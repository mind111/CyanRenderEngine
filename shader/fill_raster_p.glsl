#version 450 core

in VSOut
{
	vec2 texCoord0;
} psIn;

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform vec2 outputSize;

layout (binding = 1, offset = 0) uniform atomic_uint numFilledRasterCubes;

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

struct RasterCube {
	vec4 position;
	vec4 normal;
};

layout(std430, binding = 52) buffer RasterCubeSSBO {
    RasterCube rasterCubes[];
};

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

void main() {
	vec2 pixelCoord = gl_FragCoord.xy / outputSize;
    float depth = texture(depthBuffer, pixelCoord).r * 2.f - 1.f;
    if (depth < 0.99) {
		vec3 normal = normalize(texture(normalBuffer, pixelCoord).rgb * 2.f - 1.f);
		vec3 worldSpacePosition = screenToWorld(vec3(pixelCoord * 2.f - 1.f, depth), inverse(viewSsbo.view), inverse(viewSsbo.projection));
		// uint count = atomicCounterIncrement(numFilledRasterCubes);
		int rasterCubeIndex = int(floor(gl_FragCoord.y)) * int(outputSize.x) + int(floor(gl_FragCoord.x));
		rasterCubes[rasterCubeIndex].position = vec4(worldSpacePosition, 1.f);
		rasterCubes[rasterCubeIndex].normal = vec4(normal, 0.f);
	}
}
