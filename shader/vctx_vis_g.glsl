#version 450 core

layout (points) in;
layout (line_strip, max_vertices = 2) out;

#define pi 3.1415926

in VS_OUT
{
	vec3 p;
	vec3 dir;
} gsIn[];

out GS_OUT
{
	vec3 color;
} gsOut;

layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

//- voxel cone tracing
layout(std430, binding = 4) buffer VoxelGridData
{
    vec3 localOrigin;
    float voxelSize;
    int visMode;
    vec3 padding;
} sceneVoxelGrid;

layout(binding = 10) uniform sampler3D sceneVoxelGridAlbedo;
layout(binding = 11) uniform sampler3D sceneVoxelGridNormal;
layout(binding = 12) uniform sampler3D sceneVoxelGridRadiance;

bool isInsideBound(vec3 texCoord)
{
	if ((texCoord.x < 0.f || texCoord.x > 1.f) || (texCoord.y < 0.f || texCoord.y > 1.f) ||
		(texCoord.z < 0.f || texCoord.z > 1.f))
	{
		return false;
	}
    return true;
}

vec3 encodeHDR(vec3 color)
{
	return color / (color + vec3(1.f));
}

vec3 decodeHDR(vec3 color)
{
    return color / (vec3(1.f) - color);
}

vec3 getVoxelGridCoord(vec3 p)
{
    ivec3 voxelGridRes = textureSize(sceneVoxelGridAlbedo, 0);
    vec3 voxelGridDim = vec3(
		float(voxelGridRes.x) * sceneVoxelGrid.voxelSize,
		float(voxelGridRes.y) * sceneVoxelGrid.voxelSize,
		float(voxelGridRes.z) * sceneVoxelGrid.voxelSize
	);
    vec3 dd = p - sceneVoxelGrid.localOrigin;
    dd.z *= -1.f;
    return dd / voxelGridDim;
}

struct ConeTraceRecord
{
    vec3 radiance;
    float occ;
};

ConeTraceRecord cone60(vec3 p, vec3 rd)
{
    float alpha = 0.f;
    float occ = 0.f;
    vec3 accRadiance = vec3(0.f);
    vec3 accAlbedo = vec3(0.f);

    vec3 q = p;
	vec3 texCoord = getVoxelGridCoord(q);
    float traced = 0.f;
    float tan30 = tan(pi / 12.f);
    int numSteps = 0;

    while (alpha < 0.95f && isInsideBound(texCoord))
    {
        float coneDiameter = 2.f * traced * tan30;
        float mip = log2(max(coneDiameter / sceneVoxelGrid.voxelSize, 1.f));
        float scale = pow(4.f, int(mip));

		vec4 albedo = textureLod(sceneVoxelGridAlbedo, texCoord, mip);
        vec4 radiance = textureLod(sceneVoxelGridRadiance, texCoord, mip);
        radiance.rgb = decodeHDR(radiance.rgb);

        float opacity = albedo.a;

        // this is necessary for correcting the "darkness" caused by auto generated mipmap included empty voxels in the average
        albedo *= scale;
        albedo /= albedo.a;
        radiance *= scale;
        radiance /= radiance.a;

		// emission-absorption model front to back blending
        // todo: integrated value is too dark
		occ += (1.f - alpha) * opacity;
        accAlbedo += (1.f - alpha) * albedo.rgb;
        accRadiance += (1.f - alpha) * radiance.rgb;
		alpha += (1.f - alpha) * opacity;

        // write cube data to buffer
        float sampleVoxelSize = sceneVoxelGrid.voxelSize * pow(2.f, floor(mip));
        numSteps++;

        // marching along the ray
        float stepSize = sampleVoxelSize;
		q += rd * stepSize;
        traced += stepSize;
        texCoord = getVoxelGridCoord(q);
    }
    return ConeTraceRecord(accRadiance, occ);
}

mat3 tbn(vec3 n)
{
    vec3 up = n.y < 0.99f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 right = cross(n, up);
    vec3 forward = cross(n, right);
    return mat3(
        right,
        forward,
        n
    );
}

ConeTraceRecord coneTraceHemisphere(vec3 p, vec3 n)
{
    // offset to next voxel in normal direction
    p += n * 1.0f * sceneVoxelGrid.voxelSize;

    vec3 radiance = vec3(0.f);
    float occ = 0.f;

    // trace 5 cones distributed in the hemisphere around surface normal
    //   0, 0
    ConeTraceRecord record = cone60(p, tbn(n) * vec3(0.f, 1.f, 0.f));
    occ += record.occ;
    radiance += record.radiance;

    //  60, 0
    record = cone60(p, tbn(n) * vec3(1.7f, 0.f, 0.5f));
    occ += record.occ;
    radiance += record.radiance;

    // -60, 180
    record = cone60(p, tbn(n) * vec3(-1.7f, 0.f, 0.5f));
    occ += record.occ;
    radiance += record.radiance;

    //  60, 90
    record = cone60(p, tbn(n) * vec3(0.f, 1.7f, 0.5f));
    occ += record.occ;
    radiance += record.radiance;

    // -60, -90
    record = cone60(p, tbn(n) * vec3(0.f, -1.7f, 0.5f));
    occ += record.occ;
    radiance += record.radiance;

    radiance /= 5.f;
    occ /= 5.f;
    return ConeTraceRecord(radiance, 1.f - occ);
}

void debugConeTrace(vec3 p, vec3 n)
{
    cone60(p, gsIn[0].dir);
}

void main()
{
	gl_Position = gDrawData.projection * gDrawData.view * vec4(gsIn[0].p, 1.f);
	gsOut.color = vec3(0.7f, 0.2f, 0.2f);
	EmitVertex();

	gl_Position = gDrawData.projection * gDrawData.view * vec4(gsIn[0].p + gsIn[0].dir, 1.f);
	gsOut.color = vec3(0.7f, 0.2f, 0.2f);
	EmitVertex();

	EndPrimitive();
}
