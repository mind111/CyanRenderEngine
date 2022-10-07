#version 450 core

out vec3 outColor;

in VSOutput
{
    vec3 objectSpacePosition;
} psIn;

uniform samplerCube cubemap;
void main() {
    vec3 d = normalize(psIn.objectSpacePosition);
    outColor = texture(cubemap, d).rgb;
}
