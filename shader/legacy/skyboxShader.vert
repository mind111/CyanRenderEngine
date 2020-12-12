#version 450 core 
layout (location = 0) in vec3 vertexPos;

out vec3 samplePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    //---- Use worldPos to determine the sampling direction vector ----
    samplePos = vertexPos;
    gl_Position = projection * view * model * vec4(vertexPos, 1.0); 
    gl_Position = gl_Position.xyww;
}