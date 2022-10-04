#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D irradianceAtlas;

#define pi 3.1415926
uniform uint microBufferRes;
uniform sampler2D radianceAtlas;

void main() {
	ivec2 texCoord = ivec2(gl_WorkGroupID.x * microBufferRes, gl_WorkGroupID.y * microBufferRes);
	vec3 irradiance = vec3(0.f);
	for (int y = 0; y < microBufferRes; ++y) {
		for (int x = 0; x < microBufferRes; ++x) {
			irradiance += texelFetch(radianceAtlas, texCoord + ivec2(x, y), 0).rgb;
		}
	}
	// irradiance /= (float(microBufferRes * microBufferRes) * .5f);
	irradiance /= pi;
	imageStore(irradianceAtlas, ivec2(gl_WorkGroupID.xy), vec4(irradiance, 1.f));
}
