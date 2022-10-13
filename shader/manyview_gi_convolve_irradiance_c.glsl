#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D irradianceAtlas;

#define pi 3.1415926
uniform ivec2 hemicubeCoord;
uniform uint microBufferRes;
uniform uint resampledMicroBufferRes;
uniform samplerCube radianceCubemap;

// Returns ±1
vec2 signNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

/** Returns a unit vector. Argument o is an octahedral vector packed via octEncode,
    on the [-1, +1] square*/
vec3 octDecode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }
    return normalize(v);
}

float solidAngleHelper(vec2 st) {
    return atan(st.x * st.y, sqrt(st.x * st.x + st.y * st.y + 1.f));
}

/** note - @min: 
* reference: https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/
*/
// todo: something is wrong with this calculation
// calculate the solid angle of the pixel being rasterized
float calcCubemapTexelSolidAngle(vec3 d, float cubemapResolution) {
    float solidAngle;
    vec3 dd = abs(d);
    vec2 texCoord;
    if (dd.x > dd.y && dd.x > dd.z) {
        if (d.x > 0.f) {
            texCoord = vec2(-d.z, d.y) / dd.x;
        } else {
            texCoord = vec2(d.z, d.y) / dd.x;
        }
    }
    if (dd.y > dd.x && dd.y > dd.z) {
        if (d.y > 0.f) {
            texCoord = vec2(-d.z, -d.x) / dd.y;
        } else {
            texCoord = vec2(-d.z, d.x) / dd.y;
        }
    }
    else if (dd.z > dd.y && dd.z > dd.x) {
		if (d.z > 0.f) {
			texCoord = vec2(d.x, d.y) / dd.z;
		} else {
            texCoord = vec2(-d.x, d.y) / dd.z;
        }
    }
    float texelSize = 2.f / cubemapResolution;
    // find 4 corners of the pixel
    vec2 A = floor(texCoord * cubemapResolution * .5f) / (cubemapResolution * .5f);
    vec2 B = A + vec2(0.f, 1.f) * texelSize;
    vec2 C = A + vec2(1.f, 0.f) * texelSize;
    vec2 D = A + vec2(1.f, 1.f) * texelSize;
    solidAngle = solidAngleHelper(A) + solidAngleHelper(D) - solidAngleHelper(B) - solidAngleHelper(C);
    return solidAngle;
}

void main() {
	vec3 irradiance = vec3(0.f);
	for (int i = 0; i < resampledMicroBufferRes; ++i) {
		for (int j = 0; j < resampledMicroBufferRes; ++j) {
			vec2 texCoord = vec2(float(j) / float(resampledMicroBufferRes), float(i) / float(resampledMicroBufferRes));
			vec3 d = octDecode(texCoord * 2.f - 1.f);
            float solidAngle = calcCubemapTexelSolidAngle(d, microBufferRes);
			irradiance += texture(radianceCubemap, d).rgb * solidAngle;
		}
	}
	irradiance /= pi;
	imageStore(irradianceAtlas, ivec2(hemicubeCoord), vec4(irradiance, 1.f));
}
