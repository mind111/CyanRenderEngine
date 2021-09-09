#version 450 core

layout (location = 0) in vec4 vertexPos;

// not prefix a uniform with s_ will create an instance of lineModel
uniform mat4 lineModel;
uniform mat4 s_view;
uniform mat4 s_projection;

void main() { 
    gl_Position = s_projection * s_view * lineModel * vec4(vertexPos.xyz, 1.f);
}