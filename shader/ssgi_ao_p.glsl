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

uniform sampler2D sceneDepthTexture;
uniform sampler2D sceneNormalTexture;
// todo: maybe using the same noises from the GTAO talk will produce better results?
uniform sampler2D blueNoiseTextures_16x16_R[8];
uniform sampler2D blueNoiseTexture_1024x1024_RGBA;
uniform sampler2D AOHistoryBuffer;
uniform vec2 outputSize;
uniform int frameCount;

out vec3 outAO;

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

void main()
{
    const int numSamplesPerDirection = 4;
    // screen space coords in [0, 1]
    vec2 pixelCoord = gl_FragCoord.xy / outputSize;
    vec3 n = texture(sceneNormalTexture, pixelCoord).xyz * 2.f - 1.f;
    float xz = texture(sceneDepthTexture, pixelCoord).r;

    if (xz > 0.998)
    {
		discard;
    }

    vec3 x = screenToWorld(vec3(pixelCoord, xz) * 2.f - 1.f, inverse(view), inverse(projection));
    vec3 viewSpacePosition = (view * vec4(x, 1.f)).xyz;
    vec3 viewDirection = -normalize(viewSpacePosition);
    // world space view direction
    vec3 wo = (inverse(view) * vec4(viewDirection, 0.f)).xyz;

    // 5m sample radius
    const float worldSpaceSampleRadius = 5.f;
    const float nearClippingPlane = .1f;
    // convert world space sample radius to screen space
    float screenSpaceEffectRadius = min(worldSpaceSampleRadius / abs(viewSpacePosition.z) * .5f, .05f);
    // randomly offset the sample positions along a slice
	float offset = texture(blueNoiseTexture_1024x1024_RGBA, gl_FragCoord.xy / float(textureSize(blueNoiseTexture_1024x1024_RGBA, 0).xy)).r;
    screenSpaceEffectRadius *= offset * 0.5f + 0.5f;
    float stepSize = screenSpaceEffectRadius / numSamplesPerDirection;

    // distributing samples in space, each pixel only gets 1 sample
    float tileSize = 4.f;
    ivec2 subtileCoord = ivec2(mod(floor(gl_FragCoord.xy), tileSize));
    int sampleIndex = subtileCoord.y * int(tileSize) + subtileCoord.x;

    float visibility = 0.f;
#if 0
	// each sample corresponds to one phi slice
	for (int i = 0; i < 16; ++i) 
	{
		vec2 dir = normalize(BlueNoiseInDisk[i]);
        #if 0
		float randomRotation = texture(blueNoiseTexture, gl_FragCoord.xy / float(textureSize(blueNoiseTexture, 0).xy)).r * PI * 2.f;
		// rotate input samples
		mat2 rotation = {
			{ cos(randomRotation), sin(randomRotation) },
			{ -sin(randomRotation), cos(randomRotation) }
		};
		dir = rotation * dir;
        #endif

        float h1 = 0.f;
        float h2 = 0.f;
        float cosh1 = -1.f;
        float cosh2 = -1.f;
        vec3 sx;
        for (int j = 0; j < numSamplesPerDirection; ++j)
        {
			// calculate theta1 
            vec2 sampleCoord = pixelCoord + (j * stepSize) * dir;
            float sz1 = texture(sceneDepthTexture, sampleCoord).r;
            vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(view), inverse(projection));
            vec3 sx1 = normalize(s1 - x);
            // attenuate this sample based on distance
            float d1 = mix(dot(sx1, wo), -1.f, clamp((length(sx1) / worldSpaceSampleRadius), 0.f, 1.f));
            cosh1 = max(cosh1, d1);
            sx = sx1;

            // calculate theta2
            sampleCoord = pixelCoord - (j * stepSize) * dir;
            float sz2 = texture(sceneDepthTexture, sampleCoord).r;
            vec3 s2 = screenToWorld(vec3(sampleCoord, sz2) * 2.f - 1.f, inverse(view), inverse(projection));
            vec3 sx2 = normalize(s2 - x);
            // attenuate this sample based on distance
            float d2 = mix(dot(sx2, wo), -1.f, clamp((length(sx2) / worldSpaceSampleRadius), 0.f, 1.f));
            cosh2 = max(cosh2, d2);
        }

        h1 = -acos(cosh1);
        h2 = acos(cosh2);

        // projecting n onto current slice's plane to get np
        vec3 slicePlaneNormal = normalize(cross(sx, wo));
        vec3 np  = n - slicePlaneNormal * dot(slicePlaneNormal, n);

        // angle between np and wo
        float gamma = acos(clamp(dot(normalize(np), wo), -1.f, 1.f)) * dot(normalize(cross(wo, np)), slicePlaneNormal);

        // clamp h1, h2 to normal hemisphere
        h1 = gamma + max(-.5f * PI, h1 - gamma);
        h2 = gamma + min( .5f * PI, h2 - gamma);

        // calculate inner integral analytically
        float a = 0.25f * (-cos(2.f * h1 - gamma) + cos(gamma) + 2.f * h1 * sin(gamma)) + .25f * (-cos(2.f * h2 - gamma) + cos(gamma) + 2.f * h2 * sin(gamma));
        visibility += a * length(np);
	}
    visibility /= 16.f;
#else
    float rotation = texture(blueNoiseTextures_16x16_R[frameCount % 8], gl_FragCoord.xy / textureSize(blueNoiseTextures_16x16_R[0], 0)).r * 2.f * PI;
	mat2 rotMat = {
		{  cos(rotation), sin(rotation) },
		{ -sin(rotation), cos(rotation) }
	};
    vec2 dir = rotMat * vec2(1.f, 0.f);

	float h1 = 0.f;
	float h2 = 0.f;
	float cosh1 = -1.f;
	float cosh2 = -1.f;
	vec3 sx;
	for (int j = 0; j < numSamplesPerDirection; ++j)
	{
		// calculate theta1 
		vec2 sampleCoord = pixelCoord + (j * stepSize) * dir;
		float sz1 = texture(sceneDepthTexture, sampleCoord).r;
		vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(view), inverse(projection));
		vec3 sx1 = normalize(s1 - x);
		// attenuate this sample based on distance
		float d1 = mix(0.f, dot(sx1, wo), clamp((worldSpaceSampleRadius / length(sx1)), 0.f, 1.f));
		cosh1 = max(cosh1, d1);
		sx = sx1;

		// calculate theta2
		sampleCoord = pixelCoord - (j * stepSize) * dir;
		float sz2 = texture(sceneDepthTexture, sampleCoord).r;
		vec3 s2 = screenToWorld(vec3(sampleCoord, sz2) * 2.f - 1.f, inverse(view), inverse(projection));
		vec3 sx2 = normalize(s2 - x);
		// attenuate this sample based on distance
		float d2 = mix(0.f, dot(sx2, wo), clamp(worldSpaceSampleRadius / length(sx2), 0.f, 1.f));
		cosh2 = max(cosh2, d2);
	}

	h1 = -acos(cosh1);
	h2 = acos(cosh2);

	// projecting n onto current slice's plane to get np
	vec3 slicePlaneNormal = normalize(cross(sx, wo));
	vec3 np  = n - slicePlaneNormal * dot(slicePlaneNormal, n);

	// angle between np and wo
	float gamma = acos(clamp(dot(normalize(np), wo), -1.f, 1.f)) * dot(normalize(cross(wo, np)), slicePlaneNormal);

	// clamp h1, h2 to normal hemisphere
	h1 = gamma + max(-.5f * PI, h1 - gamma);
	h2 = gamma + min( .5f * PI, h2 - gamma);

	// calculate inner integral analytically
	float a = 0.25f * (-cos(2.f * h1 - gamma) + cos(gamma) + 2.f * h1 * sin(gamma)) + .25f * (-cos(2.f * h2 - gamma) + cos(gamma) + 2.f * h2 * sin(gamma));
	visibility += a * length(np);
#endif
    // temporal filtering
    float AOHistory = texture(AOHistoryBuffer, psIn.texCoord0).r;
    // todo: do proper reprojection and reject samples here

	outAO = vec3(AOHistory * .9f + visibility * .1f);
}
