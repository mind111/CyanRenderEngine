#version 450 core

in VertexShaderOutput
{
    vec3 objectSpacePosition;
} psIn;

out vec3 outColor;

uniform samplerCube cubemap;

void main() 
{
    vec3 d = normalize(psIn.objectSpacePosition);
    vec3 color = texture(cubemap, d).rgb;
    outColor = color / (color + 1.f);
}
