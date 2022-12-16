#version 450 core

in VSOutput
{
	vec3 objectSpacePosition;
} psIn;

out vec4 outColor;

uniform samplerCube cubemapTexture;
uniform float mipLevel;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    vec3 d = normalize(psIn.objectSpacePosition);
    vec3 linearColor = textureLod(cubemapTexture, d, mipLevel).rgb;
    outColor = vec4(linearColor, 1.0f);
}