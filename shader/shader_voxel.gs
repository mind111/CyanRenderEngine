#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform vec3 aabbMin;
uniform vec3 aabbMax;
uniform uint flag;

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} VertexIn[];

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} VertexOut;

struct OrthoInput
{
    float l;
    float r;
    float b;
    float t;
    float n;
    float f;
};

vec4 orthoProjection(vec4 v, float l, float r, float b, float t, float near, float far, float aspect)
{
    vec4 result = vec4(0., 0.f, 0.f, 1.f);
    result.x = 2.0 * (v.x - l) / (r - l) - 1.0;
    result.y = 2.0 * (v.y - b) / (t - b) - 1.0;
    result.z = (v.z - near) / (far - near);
    return result;
} 

void main()
{
    /*
        for each tri
        {
            compute face normal n,
            max(dot(n, vec3(1, 0, 0)), dot(n, vec3(0, 1, 0)).....)
            assuming decide to project along x-axis with a 512 ^ 3 voxel grid
            compute orthographic proj clong x-axis
        }
    */

    // compute face normal n,
    vec3 v0 = gl_in[0].gl_Position.xyz;
    vec3 v1 = gl_in[1].gl_Position.xyz;
    vec3 v2 = gl_in[2].gl_Position.xyz;

    float left = 0.f ;
    float right = 0.f;
    float bottom = 0.f;
    float top = 0.f;
    float near = 0.f;
    float far = 0.f;

    // orthographic projection
    for (int i = 0; i < gl_in.length(); ++i)
    {
        vec4 clipPos = vec4(0.f, 0.f, 0.f, 1.f);
        // pass-through per vertex attributes
        VertexOut.position = VertexIn[i].position;
        VertexOut.normal = VertexIn[i].normal;
        VertexOut.tangent = VertexIn[i].tangent;
        VertexOut.texCoords = VertexIn[i].texCoords;
        VertexOut.fragmentWorldPos = VertexIn[i].fragmentWorldPos;

        // x
        if (flag == 0) 
        {
            left = aabbMax.z;
            right = aabbMin.z;
            bottom = aabbMin.y;
            top = aabbMax.y;
            near = aabbMax.x;
            far = aabbMin.x;
            vec4 viewSpacePos = vec4(gl_in[i].gl_Position.z, gl_in[i].gl_Position.y, -gl_in[i].gl_Position.x, 1.f);
            clipPos = orthoProjection(viewSpacePos, left, right, bottom, top, near, far, 16.0 / 9.0);
        }
        // y
        if (flag == 1) 
        {
            left = aabbMax.z;
            right = aabbMin.z;
            bottom = aabbMax.x;
            top = aabbMin.x;
            near = aabbMax.y;
            far = aabbMin.y;
            vec4 viewSpacePos = vec4(gl_in[i].gl_Position.z, -gl_in[i].gl_Position.x, gl_in[i].gl_Position.y, 1.f);
            clipPos = orthoProjection(viewSpacePos, left, right, bottom, top, near, far, 16.0 / 9.0);
        }
        // z
        else if (flag == 2)
        {
            left = aabbMin.x;
            right = aabbMax.x;
            bottom = aabbMin.y;
            top = aabbMax.y;
            near = aabbMax.z;
            far = aabbMin.z - 0.01f;
            clipPos = orthoProjection(gl_in[i].gl_Position, left, right, bottom, top, near, far, 16.0 / 9.0);
        }

        // clipPos = orthoProjection(gl_in[i].gl_Position, left, right, bottom, top, near, far, 16.0 / 9.0);
        gl_Position = clipPos;
        EmitVertex();
    }
    EndPrimitive();
}