#version 450 core

out VsOut
{
    vec3 voxelCenterPos;
	vec4 voxelColor;
} vsOut;

const int kVisAlbedo = 1 << 0;
const int kVisOpacity = 1 << 1;
const int kVisRadiance = 1 << 2;
const int kVisNormal = 1 << 3;
const int kVisSSOpacity = 1 << 4;

//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;

layout (binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout (binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout (binding = 12) uniform sampler3D sceneVoxelGridRadiance;
layout (binding = 13) uniform sampler3D sceneVoxelGridOpacity;

uniform int activeMip;

vec3 encodeHDR(vec3 color)
{
	return color / (color + vec3(1.f));
}

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

void main()
{
	ivec3 voxelGridDim = textureSize(sceneVoxelGridAlbedo, activeMip);
	int y = gl_VertexID / (voxelGridDim.x * voxelGridDim.z);
	int xz = gl_VertexID % (voxelGridDim.x * voxelGridDim.z);
	int z = xz / voxelGridDim.x;
	int x = xz % voxelGridDim.x;
	vsOut.voxelCenterPos = sceneVoxelGrid.localOrigin + vec3(float(x) + .5f, float(y) + .5f, -(float(z) + .5f)) * sceneVoxelGrid.voxelSize * pow(2.f, activeMip);
	vec3 texCoords = (vec3(x, y, z) + .5f) / vec3(voxelGridDim);
	vec4 albedo = textureLod(sceneVoxelGridAlbedo, texCoords, activeMip);
	vec4 radiance = textureLod(sceneVoxelGridRadiance, texCoords, activeMip);
	float opacitySS = textureLod(sceneVoxelGridOpacity, texCoords, activeMip).r;

	float scale = pow(8.f, activeMip);
	if ((sceneVoxelGrid.visMode & kVisAlbedo) != 0)
	{
		albedo *= scale;
		vsOut.voxelColor = albedo.a > 0.f ? albedo / albedo.a : vec4(0.f);
	}
	else if ((sceneVoxelGrid.visMode & kVisNormal) != 0)
	{
	}
	else if ((sceneVoxelGrid.visMode & kVisRadiance) != 0)
	{
		radiance.rgb = decodeHDR(radiance.rgb);
		radiance *= scale;
		vsOut.voxelColor = radiance.a > 0.f ? radiance / radiance.a : vec4(0.f);
	}
	else if ((sceneVoxelGrid.visMode & kVisOpacity) != 0)
	{
		vsOut.voxelColor = vec4(albedo.a);
	}
	else if ((sceneVoxelGrid.visMode & kVisSSOpacity) != 0)
	{
		vsOut.voxelColor = vec4(vec3(opacitySS), 1.f);
	}
	if (vsOut.voxelColor.a > 0.f)
		gl_Position = vec4(x, y, z, 1.f);
	else
		gl_Position = vec4(x, y, z, 0.f);
}
