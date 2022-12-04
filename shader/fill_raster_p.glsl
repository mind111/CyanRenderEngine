#version 450 core

in VSOut
{
	vec2 texCoord0;
} psIn;

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform vec2 outputSize;
uniform vec2 debugRadianceCubeScreenCoord;

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

struct RadianceCube {
	vec4 position;
	vec4 normal;
};

layout(std430, binding = 52) buffer RasterCubeSSBO {
    RadianceCube radianceCubes[];
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
	int radianceCubeIndex = int(floor(gl_FragCoord.y)) * int(outputSize.x) + int(floor(gl_FragCoord.x));
    if (depth < 0.99) {
		vec3 normal = normalize(texture(normalBuffer, pixelCoord).rgb * 2.f - 1.f);
		vec3 worldSpacePosition = screenToWorld(vec3(pixelCoord * 2.f - 1.f, depth), inverse(viewSsbo.view), inverse(viewSsbo.projection));
		radianceCubes[radianceCubeIndex].position = vec4(worldSpacePosition, 1.f);
		radianceCubes[radianceCubeIndex].normal = vec4(normal, 0.f);
	}
	else {
		radianceCubes[radianceCubeIndex].position = vec4(0.f);
		radianceCubes[radianceCubeIndex].normal = vec4(0.f);
	}
}
