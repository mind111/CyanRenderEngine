#version 450 core

layout (location = 0) in vec4 vertexPos;

out flat int vertexId; 

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vertexId = 0;
    if (vertexPos.w > 1.f) 
        vertexId = 1;
    gl_Position = projection * view * model * vec4(vertexPos.xyz, 1.f);
}