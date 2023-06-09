#version 450 core

in VSOutput
{
	vec3 objectSpacePosition;
} psIn;

out vec3 outColor;

uniform samplerCube cubemap;
uniform float mipLevel;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    vec3 d = normalize(psIn.objectSpacePosition);
    vec3 linearColor = textureLod(cubemap, d, mipLevel).rgb;
    outColor = linearColor;
}