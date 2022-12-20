#version 450 core

uniform sampler2D mvgiPosition;
uniform sampler2D mvgiNormal;
uniform sampler2D mvgiAlbedo;
uniform vec2 outputSize;

struct Hemicube 
{
	vec4 position;
	vec4 normal;
};

layout(std430, binding = 52) buffer HemicubeBuffer
{
    Hemicube hemicubes[];
};

void main() 
{
	vec2 pixelCoord = gl_FragCoord.xy / outputSize;
	vec3 normal = normalize(texture(mvgiNormal, pixelCoord).rgb * 2.f - 1.f);
	vec4 position = texture(mvgiPosition, pixelCoord);
	int hemicubeIndex = int(floor(gl_FragCoord.y)) * int(outputSize.x) + int(floor(gl_FragCoord.x));
    if (position.w > 0.f) 
	{
		hemicubes[hemicubeIndex].position = vec4(position.xyz, 1.f);
		hemicubes[hemicubeIndex].normal = vec4(normal, 0.f);
	}
}
