#version 450 core
in vec3 vertexPos;
out vec3 worldPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    worldPos = (model * vec4(worldPos, 1.f)).xyz;
    gl_Position = projection * view * model * vec4(vertexPos, 1.f);
}