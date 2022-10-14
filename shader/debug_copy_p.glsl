#version 450 core

out vec3 depth;
out vec3 normal;
out vec3 sceneColor;

uniform sampler2D srcDepthBuffer;
uniform sampler2D srcNormalBuffer;
uniform sampler2D srcSceneColor;

void main() {
	vec2 texCoord = gl_FragCoord.xy / textureSize(srcDepthBuffer, 0).xy;
	depth = texture(srcDepthBuffer, texCoord).rgb;
	normal = texture(srcNormalBuffer, texCoord).rgb;
	sceneColor = texture(srcSceneColor, texCoord).rgb;
}
