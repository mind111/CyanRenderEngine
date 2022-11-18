#version 450 core

#define MICRO_BUFFER_RES_X 16
#define MICRO_BUFFER_RES_Y 16

layout (local_size_x = MICRO_BUFFER_RES_X, local_size_y = MICRO_BUFFER_RES_Y, local_size_z = 1) in;
layout(rgba16f, binding = 0) uniform image2D microColorBuffer;

struct SurfelBSHNode {
	vec4 center;
	vec4 normal;
	vec4 albedo;
	vec4 radiance;
	vec4 coneAperture;
	vec4 radius;
	ivec4 childs;
};

layout (std430, binding = 64) buffer SurfelBSH {
	SurfelBSHNode nodes[];
};

mat4 view;
mat4 projection;

/** note - @min: 
* reference: https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/
*/
// calculate the solid angle of the pixel being rasterized
float solidAngleHelper(vec2 st) {
    return atan(st.x * st.y, sqrt(st.x * st.x + st.y * st.y + 1.f));
}

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

bool isLeafNode(in SurfelBSHNode node) {
	return (node.childs.x < 0 && node.childs.y < 0);
}

bool isInViewport(in SurfelBSHNode node, out vec2 pixelCoord) {
	vec4 pos = projection * view * node.center;
	pos /= pos.w;
	pixelCoord = pos.xy * .5f + .5f;
	return (pixelCoord.x >= 0.f && pixelCoord.x <= 1.f && pixelCoord.y >= 0.f && pixelCoord.y <= 1.f);
}

#define M_PI 3.14159265359
#define INFINITY (1.f / 0.f)
const int kNumPixels = MICRO_BUFFER_RES_X * MICRO_BUFFER_RES_Y;
float microDepthBuffer[kNumPixels] = float[kNumPixels](INFINITY);
int microNodeIndexBuffer[kNumPixels] = int[kNumPixels](-1);

void drawNodeToPixel(in vec2 pixelCoord, int nodeIndex, float dist) {
    SurfelBSHNode node = nodes[nodeIndex];
	int x = int(floor(pixelCoord.x * MICRO_BUFFER_RES_X));
	int y = int(floor(pixelCoord.y * MICRO_BUFFER_RES_Y));
	int pixelIndex = y * MICRO_BUFFER_RES_X + MICRO_BUFFER_RES_X;
	if (dist < microDepthBuffer[pixelIndex]) {
		microDepthBuffer[pixelIndex] = dist;
		microNodeIndexBuffer[pixelIndex] = nodeIndex;
		imageStore(microColorBuffer, ivec2(x, y), vec4(node.albedo.rgb, 1.f));
	}
}

// todo: do proper frustum culling
void renderInternal(int nodeIndex) {
	if (nodeIndex < 0) return;

    SurfelBSHNode node = nodes[nodeIndex];
	vec3 cameraPos = -view[3].xyz;
	vec3 v = node.center.xyz - cameraPos;
	float dist = length(v);
	vec2 pixelCoord;
    if (!isInViewport(node, pixelCoord)) {
		renderInternal(node.childs.x);
		renderInternal(node.childs.y);
    }
    else {
		if (isLeafNode(node)) {
			drawNodeToPixel(pixelCoord, nodeIndex, dist);
		}
		else {
			float nodeSolidAngle = (M_PI * node.radius.x * node.radius.x) * max(dot(node.normal.xyz, normalize(v)), 0.f) / (dist * dist);
			float texelSolidAngle = calcCubemapTexelSolidAngle(normalize(v), float(MICRO_BUFFER_RES_X));
			bool stopRecursion = (nodeSolidAngle <= (texelSolidAngle * 1.2f));
			// stop recursion
			if (stopRecursion) {
				drawNodeToPixel(pixelCoord, nodeIndex, dist);
			}
			else {
				// continue recursion
				renderInternal(node.childs.x);
				renderInternal(node.childs.y);
			}
		}
    }
}

void render() {
    renderInternal(0);
}

void main() {
    render();
}
