#version 450 core

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	float cull;
} vsOut;

uniform sampler3D u_voxelGridOpacityTex;
uniform vec3 u_voxelGridResolution;
uniform vec3 u_voxelGridCenter;
uniform vec3 u_voxelGridExtent;
uniform vec3 u_voxelSize;

void main()
{
	vsOut.cull = 0.f;

	int x = int(u_voxelGridResolution.x);
	int y = int(u_voxelGridResolution.y);
	int z = int(u_voxelGridResolution.z);
	int voxelCoordZ = gl_VertexID / (x * y);
	int voxelCoordY = gl_VertexID % (x * y) / x;
	int voxelCoordX = gl_VertexID % (x * y) % x;

	float opacity = texelFetch(u_voxelGridOpacityTex, ivec3(voxelCoordX, voxelCoordY, voxelCoordZ), 0).r;
	if (opacity < .5f)
	{
		vsOut.cull = 1.f;
		return;
	}

	vec3 voxelCoord = vec3(voxelCoordX, voxelCoordY, voxelCoordZ)  + .5f;
	vec3 voxelGridOrigin = u_voxelGridCenter + vec3(-u_voxelGridExtent.xy, u_voxelGridExtent.z);
	vec3 voxelCenterPosition = voxelGridOrigin + vec3(voxelCoord.x * u_voxelSize.x, voxelCoord.y * u_voxelSize.y, -voxelCoord.z * u_voxelSize.z);
	gl_Position = vec4(voxelCenterPosition, 1.f);
}
