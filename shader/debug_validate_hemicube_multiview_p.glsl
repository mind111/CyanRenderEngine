#version 450 core

out vec3 outColor;
uniform vec3 color;
uniform samplerCube hemicubeMultiView; 

in VSOutput
{
    vec4 color;
	vec3 objectSpacePosition;
} psIn;

uniform uint microBufferRes;
uniform ivec2 hemicubeIndex;
vec3 sampleHemicubeMultiView(vec3 d) {
	vec2 hemicubeFaceSize = vec2(1.f) / 4.f;
    vec3 sampleColor = vec3(0.f);
    vec3 dd = abs(d);
    if (dd.x > dd.y && dd.x > dd.z) {
        d *= 1.f / dd.x;
        vec3 radiance = vec3(0.f);
        if (d.x > 0.f) {
            vec3 u = vec3(0.f, 0.f, -2.f);
            vec3 v = vec3(0.f, 2.f,  0.f);
            vec3 origin = vec3(1.f, -1.f, 1.f);
            vec2 localCoord = hemicubeFaceSize * (vec2(-d.z, d.y) * .5f + .5f);
            vec2 uv = hemicubeFaceSize * vec2(0.f, 3.f) + localCoord;
            vec3 sampleDir = origin + uv.x * u + uv.y * v;
            sampleColor = texture(hemicubeMultiView, sampleDir).rgb;
        } else {
            vec3 u = vec3(0.f, 0.f,  2.f);
            vec3 v = vec3(0.f, 2.f,  0.f);
            vec3 origin = vec3(-1.f, -1.f, -1.f);
            vec2 localCoord = hemicubeFaceSize * (vec2(d.z, d.y) * .5f + .5f);
            vec2 uv = hemicubeFaceSize * vec2(0.f, 3.f) + localCoord;
            vec3 sampleDir = origin + uv.x * u + uv.y * v;
            sampleColor = texture(hemicubeMultiView, sampleDir).rgb;
        }
    }
    if (dd.y > dd.x && dd.y > dd.z) {
        if (d.y > 0.f) {
        } else {
        }
    }
    else if (dd.z > dd.y && dd.z > dd.x) {
		if (d.z > 0.f) {
		} else {
        }
    }
    return sampleColor;
}

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

	outColor = sampleHemicubeMultiView(normalize(psIn.objectSpacePosition)) * colorGradient;
}
