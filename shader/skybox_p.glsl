#version 450 core

in VSOutput
{
	vec3 objectSpacePosition;
} psIn;

out vec4 outColor;
layout (location = 0) out vec4 linearColor;
uniform samplerCube cubemapTexture;

float saturate(float k)
{
    return clamp(k, 0.f, 1.f);
}

void main()
{
    vec3 d = normalize(psIn.objectSpacePosition);
    vec3 linearColor = texture(cubemapTexture, d).rgb;
    outColor = vec4(linearColor, 1.0f);
}