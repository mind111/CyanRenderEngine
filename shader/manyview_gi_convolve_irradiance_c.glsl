#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D irradianceAtlas;

#define pi 3.1415926
uniform ivec2 texCoord;
uniform uint finalGatherRes;
uniform uint radianceRes;
uniform samplerCube radianceCubemap;
uniform vec3 hemicubeNormal;
uniform mat4 hemicubeTangentFrame;

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
	for (int i = 0; i < radianceRes; ++i) {
		for (int j = 0; j < radianceRes; ++j) {
			vec2 pixelCoord = (vec2(float(j), float(i)) + .5f) / radianceRes;
			vec3 d = octDecode(pixelCoord * 2.f - 1.f);
            vec3 worldSpaceDir = (hemicubeTangentFrame * vec4(d, 0.f)).xyz;
            float ndotl = max(dot(worldSpaceDir, hemicubeNormal), 0.f);
			/** note - @mind:
			* dividing by finalGatherRes here might not be right here as a texel
            * in the hemicube doesn't exactly corresponds to one texel in the resampled
            * radiance atlas, but leave it as is for now
			*/
            float solidAngle = calcCubemapTexelSolidAngle(d, finalGatherRes);
			irradiance += texture(radianceCubemap, d).rgb * ndotl * solidAngle;
		}
	}
	irradiance /= pi;
	imageStore(irradianceAtlas, ivec2(texCoord), vec4(irradiance, 1.f));
}
