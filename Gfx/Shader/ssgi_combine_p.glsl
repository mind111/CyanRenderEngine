#version 450 core

in VSOutput
{
	vec2 texCoord0;
} psIn;

out vec3 outColor;

uniform sampler2D u_indirectLightingTex; 
uniform sampler2D u_SSGISceneColor; 

void main()
{
	outColor = texture(u_indirectLightingTex, psIn.texCoord0).rgb + texture(u_SSGISceneColor, psIn.texCoord0).rgb;
}
