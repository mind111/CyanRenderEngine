#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

uniform vec3 aabbMin;
uniform vec3 aabbMax;
uniform uint axis;

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} gsIn[];

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
    vec3 fragmentWorldPos;
} gsOut;

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

    // orthographic projection
    for (int i = 0; i < gl_in.length(); ++i)
    {
        vec4 clipPos = vec4(0.f, 0.f, 0.f, 1.f);
        // pass-through per vertex attributes
        gsOut.position = gsIn[i].position;
        gsOut.normal = gsIn[i].normal;
        gsOut.tangent = gsIn[i].tangent;
        gsOut.texCoords = gsIn[i].texCoords;
        gsOut.fragmentWorldPos = gsIn[i].fragmentWorldPos;

        // x
        if (axis == 0) 
        {
            float left = aabbMax.z;
            float right = aabbMin.z;
            float bottom = aabbMin.y;
            float top = aabbMax.y;
            float near = aabbMax.x;
            float far = aabbMin.x;
            vec4 viewSpacePos = vec4(-gl_in[i].gl_Position.z, gl_in[i].gl_Position.y, gl_in[i].gl_Position.x, 1.f);
            clipPos = orthoProjection(viewSpacePos, left, right, bottom, top, near, far);
        }
        // y
        else if (axis == 1) 
        {
            float left = aabbMax.z;
            float right = aabbMin.z;
            float bottom = aabbMax.x;
            float top = aabbMin.x;
            float near = aabbMax.y;
            float far = aabbMin.y;
            vec4 viewSpacePos = vec4(-gl_in[i].gl_Position.z, -gl_in[i].gl_Position.x, gl_in[i].gl_Position.y, 1.f);
            clipPos = orthoProjection(viewSpacePos, left, right, bottom, top, near, far);
        }
        // z
        else if (axis == 2)
        {
            float left = aabbMin.x;
            float right = aabbMax.x;
            float bottom = aabbMin.y;
            float top = aabbMax.y;
            float near = aabbMax.z;
            // float far = aabbMin.z - 0.01f;
            float far = aabbMin.z;
            clipPos = orthoProjection(gl_in[i].gl_Position, left, right, bottom, top, near, far);
        }

        gl_Position = clipPos;
        EmitVertex();
    }
    EndPrimitive();
}