#version 450 core

#extension GL_NV_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable 

out vec3 outColor;

in VSOutput
{
    flat int instanceID;
	vec3 objectSpacePosition;
} psIn;

layout(std430, binding = 64) buffer CubemapHandleBuffer {
    uint64_t handles[];
};

uniform uint microBufferRes;

void main() {
    samplerCube cubemap = samplerCube(handles[psIn.instanceID]);
    vec3 pixelDir = normalize(psIn.objectSpacePosition);
    vec3 color = texture(cubemap, pixelDir).rgb;

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

	outColor = color * colorGradient;
}
