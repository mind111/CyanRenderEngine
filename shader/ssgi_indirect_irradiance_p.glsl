#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

layout (std430) buffer ViewBuffer
{
    mat4 view;
    mat4 projection;
};

/**
* blue noise samples on a unit disk taken from https://www.shadertoy.com/view/3sfBWs
*/
const vec2 BlueNoiseInDisk[64] = vec2[64](
    vec2(0.478712,0.875764),
    vec2(-0.337956,-0.793959),
    vec2(-0.955259,-0.028164),
    vec2(0.864527,0.325689),
    vec2(0.209342,-0.395657),
    vec2(-0.106779,0.672585),
    vec2(0.156213,0.235113),
    vec2(-0.413644,-0.082856),
    vec2(-0.415667,0.323909),
    vec2(0.141896,-0.939980),
    vec2(0.954932,-0.182516),
    vec2(-0.766184,0.410799),
    vec2(-0.434912,-0.458845),
    vec2(0.415242,-0.078724),
    vec2(0.728335,-0.491777),
    vec2(-0.058086,-0.066401),
    vec2(0.202990,0.686837),
    vec2(-0.808362,-0.556402),
    vec2(0.507386,-0.640839),
    vec2(-0.723494,-0.229240),
    vec2(0.489740,0.317826),
    vec2(-0.622663,0.765301),
    vec2(-0.010640,0.929347),
    vec2(0.663146,0.647618),
    vec2(-0.096674,-0.413835),
    vec2(0.525945,-0.321063),
    vec2(-0.122533,0.366019),
    vec2(0.195235,-0.687983),
    vec2(-0.563203,0.098748),
    vec2(0.418563,0.561335),
    vec2(-0.378595,0.800367),
    vec2(0.826922,0.001024),
    vec2(-0.085372,-0.766651),
    vec2(-0.921920,0.183673),
    vec2(-0.590008,-0.721799),
    vec2(0.167751,-0.164393),
    vec2(0.032961,-0.562530),
    vec2(0.632900,-0.107059),
    vec2(-0.464080,0.569669),
    vec2(-0.173676,-0.958758),
    vec2(-0.242648,-0.234303),
    vec2(-0.275362,0.157163),
    vec2(0.382295,-0.795131),
    vec2(0.562955,0.115562),
    vec2(0.190586,0.470121),
    vec2(0.770764,-0.297576),
    vec2(0.237281,0.931050),
    vec2(-0.666642,-0.455871),
    vec2(-0.905649,-0.298379),
    vec2(0.339520,0.157829),
    vec2(0.701438,-0.704100),
    vec2(-0.062758,0.160346),
    vec2(-0.220674,0.957141),
    vec2(0.642692,0.432706),
    vec2(-0.773390,-0.015272),
    vec2(-0.671467,0.246880),
    vec2(0.158051,0.062859),
    vec2(0.806009,0.527232),
    vec2(-0.057620,-0.247071),
    vec2(0.333436,-0.516710),
    vec2(-0.550658,-0.315773),
    vec2(-0.652078,0.589846),
    vec2(0.008818,0.530556),
    vec2(-0.210004,0.519896) 
);

uniform sampler2D sceneDepthTexture;
uniform sampler2D sceneNormalTexture;
uniform sampler2D diffuseRadianceBuffer;
uniform sampler2D blueNoiseTexture_1024x1024_RGBA;
uniform vec2 outputSize;
uniform float normalErrorTolerance;

out vec3 outIndirectIrradiance;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) 
{
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 worldToScreen(vec3 pp, in mat4 view, in mat4 projection)
{
    vec4 p = projection * view * vec4(pp, 1.f);
    p /= p.w;
    return p.xyz * .5f + .5f;
}

#define DEPTH_DELTA_EPSILON 0.005f

float intersectScreenRect(vec2 screenSpaceRo, vec2 screenSpaceRd)
{
	float txLeft = (screenSpaceRo.x - 0.f) / screenSpaceRd.x;
	float txRight = (screenSpaceRo.x - 1.f) / screenSpaceRd.x;
    float tx = max(txLeft, txRight);

	float tyBottom = (screenSpaceRo.y - 0.f) / screenSpaceRd.y;
	float tyTop = (screenSpaceRo.y - 1.f) / screenSpaceRd.y;
    float ty = max(tyBottom, tyTop);
    return min(tx, ty);
}

void main()
{
    const int numSamplesPerDirection = 8; // 2 x numSamplesPerDirection of taps per slice
    // screen space coords in [0, 1]
    vec2 pixelCoord = gl_FragCoord.xy / outputSize;
    float xz = texture(sceneDepthTexture, pixelCoord).r;
    if (xz > 0.998)
    {
		discard;
    }
    vec3 n = texture(sceneNormalTexture, pixelCoord).xyz * 2.f - 1.f;

    vec3 x = screenToWorld(vec3(pixelCoord, xz) * 2.f - 1.f, inverse(view), inverse(projection));
    vec3 viewSpacePosition = (view * vec4(x, 1.f)).xyz;
    vec3 viewDirection = -normalize(viewSpacePosition);
    // world space view direction
    vec3 wo = (inverse(view) * vec4(viewDirection, 0.f)).xyz;

    // 5m sample radius
    const float worldSpaceSampleRadius = 10.f;
    const float nearClippingPlane = .1f;
    // convert world space sample radius to screen space
    float screenSpaceEffectRadius = min(worldSpaceSampleRadius / abs(viewSpacePosition.z) * .5f, .2f);
    // randomly offset the sample positions along a slice
	float offset = texture(blueNoiseTexture_1024x1024_RGBA, gl_FragCoord.xy / float(textureSize(blueNoiseTexture_1024x1024_RGBA, 0).xy)).r;
    screenSpaceEffectRadius *= offset * 0.5f + 0.5f;
    float stepSize = screenSpaceEffectRadius / numSamplesPerDirection;

    vec3 indirectIrradiance = vec3(0.f);

    const float uniformScreenSpaceStepSize = .1f;

    const int numSampleSlices = 16;
    // distributing samples in space, each pixel only gets 1 sample
    float tileSize = 8.f;
    ivec2 subtileCoord = ivec2(mod(floor(gl_FragCoord.xy), tileSize));
    int sampleIndex = subtileCoord.y * int(tileSize) + subtileCoord.x;
	for (int i = 0; i < numSampleSlices; ++i)
    {
		vec2 dir = normalize(BlueNoiseInDisk[i]);
		float rotation = texture(blueNoiseTexture_1024x1024_RGBA, gl_FragCoord.xy / textureSize(blueNoiseTexture_1024x1024_RGBA, 0)).r * 2.f * PI;
		mat2 rotMat2 = {
			{  cos(rotation), sin(rotation) },
			{ -sin(rotation), cos(rotation) }
		};
        dir = rotMat2 * dir;

        // solve this slice's intersection with the screen rect
        // determine num of steps to march in screen space based on uniform step size
        int numSteps1 = int(floor(intersectScreenRect(pixelCoord, dir) / uniformScreenSpaceStepSize));
        int numSteps2 = int(floor(intersectScreenRect(pixelCoord, -dir) / uniformScreenSpaceStepSize));

		vec3 cameraRightVector = vec3(view[0][0], view[1][0], view[2][0]);
		vec3 cameraUpVector = vec3(view[0][1], view[1][1], view[2][1]);
		// slice plane x axis
		vec3 slicePlaneXAxis = dir.x * cameraRightVector + dir.y * cameraUpVector;

		// projecting n onto current slice's plane to get np
		vec3 slicePlaneNormal = normalize(cross(slicePlaneXAxis, wo));
		vec3 np  = n - slicePlaneNormal * dot(slicePlaneNormal, n);
		// angle between np and wo
		float gamma = acos(clamp(dot(normalize(np), wo), -1.f, 1.f)) * dot(normalize(cross(wo, np)), slicePlaneNormal);

		float h1 = -PI; // h1 is clock-wise thus negative
		float h2 = PI;  // h2 is counterclock-wise thus positive

		// start the horizon angle at the normal plane
		h1 = gamma + max(-.5f * PI, h1 - gamma);
		h2 = gamma + min( .5f * PI, h2 - gamma);

		float prevT1 = h1;
		float prevT2 = h2;

#if 1
        float theta1; 
        vec3 radiance1, n1, sx1;
        for (int j = 0; j < numSteps1; ++j)
        {
			vec2 sampleCoord = pixelCoord + (j * uniformScreenSpaceStepSize) * dir;

			// calculate theta1 
			float sz1 = texture(sceneDepthTexture, sampleCoord).r;
            n1 = normalize(texture(sceneNormalTexture, sampleCoord).xyz * 2.f - 1.f);
			radiance1 = texture(diffuseRadianceBuffer, sampleCoord).rgb;
			vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(view), inverse(projection));
			sx1 = s1 - x;

			theta1 = -acos(dot(normalize(sx1), wo));
			h1 = max(h1, theta1);
        }

        float theta2;
        vec3 radiance2, n2, sx2;
        for (int j = 0; j < numSteps2; ++j)
        {
			// calculate theta2
			vec2 sampleCoord = pixelCoord - (j * uniformScreenSpaceStepSize) * dir;

			float sz2 = texture(sceneDepthTexture, sampleCoord).r;
            n2 = normalize(texture(sceneNormalTexture, sampleCoord).xyz * 2.f - 1.f);
			radiance2 = texture(diffuseRadianceBuffer, sampleCoord).rgb;
			vec3 s2 = screenToWorld(vec3(sampleCoord, sz2) * 2.f - 1.f, inverse(view), inverse(projection));
			sx2 = s2 - x;

			theta2 = acos(dot(normalize(sx2), wo));
			h2 = min(h2, theta2);
        }

		// if the horizon angle sample "rises", then integrate indirect lighting
		float nx = sin(gamma);
		float ny = cos(gamma);

		if (theta1 > prevT1)
		{
			float t0 = prevT1;
			float t1 = theta1;
			float cost0 = cos(t0);
			float cost1 = cos(t1);
			float normalCorrection = smoothstep(normalErrorTolerance, 0.f, dot(n1, normalize(-sx1)));
			indirectIrradiance += radiance1 * (.5f * ny * (t1 - t0 + sin(t0) * cost0 - sin(t1) * cost1) + .5f * nx * (cost0 * cost0 - cost1 * cost1)) * normalCorrection;

			prevT1 = theta1;
		}
		if (theta2 < prevT2)
		{
			float t0 = theta2;
			float t1 = prevT2;
			float cost0 = cos(t0);
			float cost1 = cos(t1);
			float normalCorrection = smoothstep(normalErrorTolerance, 0.f, dot(n2, normalize(-sx2)));
			indirectIrradiance += radiance2 * (.5f * nx * (t1 - t0 + sin(t0) * cost0 - sin(t1) * cost1) + .5f * ny * (cost0 * cost0 - cost1 * cost1)) * normalCorrection;

			prevT2 = theta2;
		}
#else
		// todo: solve the ray's intersection with the screen rect to determine sample boundry
        // todo: define a uniform screen space step size, and use each ray's intersection with the screen rect to determine how many steps to take for each ray

		for (int j = 0; j < numSamplesPerDirection; ++j)
		{
			vec2 sampleCoord = pixelCoord + (j * stepSize) * dir;

			// calculate theta1 
			float sz1 = texture(sceneDepthTexture, sampleCoord).r;
            vec3 n1 = normalize(texture(sceneNormalTexture, sampleCoord).xyz * 2.f - 1.f);
			vec3 radiance1 = texture(diffuseRadianceBuffer, sampleCoord).rgb;
			vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(view), inverse(projection));
			vec3 sx1 = s1 - x;

			float theta1 = -acos(dot(normalize(sx1), wo));
			h1 = max(h1, theta1);

			// calculate theta2
			sampleCoord = pixelCoord - (j * stepSize) * dir;

			float sz2 = texture(sceneDepthTexture, sampleCoord).r;
            vec3 n2 = normalize(texture(sceneNormalTexture, sampleCoord).xyz * 2.f - 1.f);
			vec3 radiance2 = texture(diffuseRadianceBuffer, sampleCoord).rgb;
			vec3 s2 = screenToWorld(vec3(sampleCoord, sz2) * 2.f - 1.f, inverse(view), inverse(projection));
			vec3 sx2 = s2 - x;

			float theta2 = acos(dot(normalize(sx2), wo));
			h2 = min(h2, theta2);

			// if the horizon angle sample "rises", then integrate indirect lighting
			float nx = sin(gamma);
			float ny = cos(gamma);

			if (theta1 > prevT1)
			{
				float t0 = prevT1;
				float t1 = theta1;
				float cost0 = cos(t0);
				float cost1 = cos(t1);
                float normalCorrection = smoothstep(normalErrorTolerance, 0.f, dot(n1, normalize(-sx1)));
				indirectIrradiance += radiance1 * (.5f * ny * (t1 - t0 + sin(t0) * cost0 - sin(t1) * cost1) + .5f * nx * (cost0 * cost0 - cost1 * cost1));

				prevT1 = theta1;
			}
			if (theta2 < prevT2)
			{
				float t0 = theta2;
				float t1 = prevT2;
				float cost0 = cos(t0);
				float cost1 = cos(t1);
                float normalCorrection = smoothstep(normalErrorTolerance, 0.f, dot(n2, normalize(-sx2)));
				indirectIrradiance += radiance2 * (.5f * nx * (t1 - t0 + sin(t0) * cost0 - sin(t1) * cost1) + .5f * ny * (cost0 * cost0 - cost1 * cost1));

                prevT2 = theta2;
			}
		}
#endif
    }

    indirectIrradiance /= numSampleSlices;
    outIndirectIrradiance = indirectIrradiance;
}
