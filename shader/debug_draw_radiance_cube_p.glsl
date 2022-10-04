#version 450 core

out vec3 outColor;
uniform vec3 color;
uniform samplerCube debugRadianceCube; 

in VSOutput
{
    vec4 color;
	vec3 objectSpacePosition;
} psIn;

uniform uint microBufferRes;

void main() {
	outColor = psIn.color.rgb;
    vec3 pixelDir = normalize(psIn.objectSpacePosition);

    // add color gradient to each radiance cube texel as a neat visualization
	float texelSideLen = 2.f / float(microBufferRes);
    float colorGradient = 1.f;
    if (abs(psIn.objectSpacePosition.x) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.yz / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.yz - texelCenterCoord) / (.8 * texelSideLen);
    }
    if (abs(psIn.objectSpacePosition.y) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.xz / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.xz - texelCenterCoord) / (.8 * texelSideLen);
    }
    else if (abs(psIn.objectSpacePosition.z) > 0.99f) {
        vec2 texelCenterCoord = (floor(psIn.objectSpacePosition.xy / texelSideLen) + .5f) * texelSideLen;
        colorGradient = 1.f - length(psIn.objectSpacePosition.xy - texelCenterCoord) / (.8 * texelSideLen);
    }

	outColor = texture(debugRadianceCube, normalize(psIn.objectSpacePosition)).rgb * colorGradient;
}
