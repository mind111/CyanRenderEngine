#version 450 core

in VSOutput
{
	vec3 objectSpacePosition;
} psIn;

out vec3 outColor;

uniform samplerCube u_cubemapTex;
uniform float u_mipLevel;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    vec3 d = normalize(psIn.objectSpacePosition);
    vec3 linearColor = textureLod(u_cubemapTex, d, u_mipLevel).rgb;
    outColor = linearColor;
}
