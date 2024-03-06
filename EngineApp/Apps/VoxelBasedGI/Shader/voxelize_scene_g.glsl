#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoord;
	vec3 albedo;
	float roughness;
	float metallic;
} gsIn[];

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoord;
	vec3 albedo;
	float roughness;
	float metallic;
} gsOut;

uniform vec3 u_voxelGridAABBMin;
uniform vec3 u_voxelGridAABBMax;

vec4 orthoProjection(vec4 v, float l, float r, float b, float t, float near, float far)
{
    vec4 result = vec4(0., 0.f, 0.f, 1.f);
    result.x = 2.0 * (v.x - l) / (r - l) - 1.0;
    result.y = 2.0 * (v.y - b) / (t - b) - 1.0;
    result.z = (v.z - near) / (far - near);
    return result;
} 

void main()
{
    // compute face normal n,
    vec3 v0 = gl_in[0].gl_Position.xyz;
    vec3 v1 = gl_in[1].gl_Position.xyz;
    vec3 v2 = gl_in[2].gl_Position.xyz;

    vec3 fn = normalize(cross(v1 - v0, v2 - v0));

    int axis = -1;
    // select dominant axis
    float ndotx = abs(dot(fn, vec3(1.f, 0.f, 0.f)));
    float ndoty = abs(dot(fn, vec3(0.f, 1.f, 0.f)));
    float ndotz = abs(dot(fn, vec3(0.f, 0.f, 1.f)));

    if (ndotx >= ndoty && ndotx >= ndotz)
    {
        axis = 0;
    }
    else if (ndoty >= ndotx && ndoty >= ndotz)
    {
        axis = 1;
    }
    else 
    {
        axis = 2;
    }

    float left, right, bottom, top, near, far;

    // orthographic projection
    for (int i = 0; i < gl_in.length(); ++i)
    {
		vec4 viewSpacePosition;
		// x
		if (axis == 0) 
		{
			left = -u_voxelGridAABBMax.z;
			right = -u_voxelGridAABBMin.z;
			bottom = u_voxelGridAABBMin.y;
			top = u_voxelGridAABBMax.y;
			near = u_voxelGridAABBMax.x;
			far = u_voxelGridAABBMin.x;

			viewSpacePosition = vec4(-gsIn[i].position.z, gsIn[i].position.y, gsIn[i].position.x, 1.f);
		}
		// y
		else if (axis == 1) 
		{
			left = u_voxelGridAABBMin.x;
			right = u_voxelGridAABBMax.x;
			bottom = -u_voxelGridAABBMax.z;
			top = -u_voxelGridAABBMin.z;
			near = u_voxelGridAABBMax.y;
			far = u_voxelGridAABBMin.y;

			viewSpacePosition = vec4(gsIn[i].position.x, -gsIn[i].position.z, gsIn[i].position.y, 1.f);
		}
		// z
		else if (axis == 2)
		{
			left = u_voxelGridAABBMin.x;
			right = u_voxelGridAABBMax.x;
			bottom = u_voxelGridAABBMin.y;
			top = u_voxelGridAABBMax.y;
			near = u_voxelGridAABBMax.z;
			far = u_voxelGridAABBMin.z;

			viewSpacePosition = vec4(gsIn[i].position.xyz, 1.f);
		}

        // pass-through per vertex attributes
        gsOut.position = gsIn[i].position;
        gsOut.normal = gsIn[i].normal;
        gsOut.tangent = gsIn[i].tangent;
        gsOut.texCoord = gsIn[i].texCoord;
        gsOut.albedo = gsIn[i].albedo;
        gsOut.roughness = gsIn[i].roughness;
        gsOut.metallic = gsIn[i].metallic;

        vec4 clipSpacePosition = orthoProjection(viewSpacePosition, left, right, bottom, top, near, far);
        gl_Position = clipSpacePosition;
		gl_ViewportIndex = axis;

        EmitVertex();
    }
    EndPrimitive();
}
