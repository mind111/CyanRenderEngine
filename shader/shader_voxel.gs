#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VertexData
{
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 texCoords;
} VertexIn[3];

out VertexData
{
    vec3 normal;
    vec3 tangent;
    vec3 texCoords;
} VertexOut;

// aabb need to be transformed to view space
uniform vec3 aabbMin;
uniform vec3 aabbMax;

mat4 ortho(float l, float r, float b, float t, float n, float f)
{
    vec4 col0 = vec4(2.0f/(r - l), 0.f, 0.f, 0.f);
    vec4 col1 = vec4(0.f, 2.f/(t - b),  0.f,  0.f);
    vec4 col2 = vec4(0.f, 0.f, -2.f/(f - n), 0.f);
    vec4 col3 = vec4(-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1.0f);
    return mat4(col0, col1, col2, col3);
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

    // assuming counter-clockwise winding order
    vec3 fn = cross(v1 - v0, v2 - v0);
    // find which axis to use for projection
    int flag = 1;
    if (abs(fn.y) > abs(fn.x) && abs(fn.y) > abs(fn.z))
    {
        flag = 2;
    }
    else if (abs(fn.z) > abs(fn.x))
    {
        flag = 3;
    }

    // orthographic projection
    for (int i = 0; i < gl_in.length(); ++i)
    {
        vec4 clipPos = vec4(0.f, 0.f, 0.f, 1.f);
        // pass-through per vertex attributes
        VertexOut.normal = VertexIn[i].normal;
        VertexOut.tangent = VertexIn[i].tangent;
        VertexOut.texCoords = VertexIn[i].texCoords;
        
        float near = -0.2f;
        float far = -100.f;

        mat4 proj;
        // x
        if (flag == 1) 
        {
            proj = ortho(aabbMin.z, aabbMax.z, aabbMin.y, aabbMax.y, aabbMax.x, aabbMin.x);
        }
        // y
        else if (flag == 2)
        {
            proj = ortho(aabbMin.x, aabbMax.x, aabbMin.z, aabbMax.z, aabbMax.y, aabbMin.y);
        }
        // z
        else if (flag == 3)
        {
            proj = ortho(aabbMin.x, aabbMax.x, aabbMin.y, aabbMax.y, aabbMax.z, aabbMin.z);
        }
        clipPos = proj * vec4(gl_in[i].gl_Position.xyz, 1.0f);
        gl_Position = vec4(clipPos.xyz, 1.0f);
        EmitVertex();
    }
    EndPrimitive();
}