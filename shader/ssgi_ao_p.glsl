#version 450 core

#define PI 3.1415926

in VSOutput
{
	vec2 texCoord0;
} psIn;

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
// todo: maybe using the same noises from the GTAO talk will produce better results?
uniform sampler2D blueNoise_16x16_R[8];
uniform sampler2D blueNoise_1024x1024_RGBA;

uniform sampler2D AOHistoryBuffer;

uniform mat4 prevFrameView;
uniform mat4 prevFrameProjection;
uniform sampler2D prevFrameSceneDepthTexture;

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

#define DEPTH_DELTA_EPSILON 0.005f

void main()
{
    const int numSamplesPerDirection = 4; // 2 x numSamplesPerDirection of taps per slice
    // screen space coords in [0, 1]
    vec2 pixelCoord = gl_FragCoord.xy / viewParameters.renderResolution;
    float xz = texture(sceneDepth, pixelCoord).r;
    vec3 n = texture(sceneNormal, pixelCoord).xyz * 2.f - 1.f;

    if (xz > 0.999999)
    {
		discard;
    }

    vec3 x = screenToWorld(vec3(pixelCoord, xz) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
    vec3 viewSpacePosition = (viewParameters.viewMatrix * vec4(x, 1.f)).xyz;
    vec3 viewDirection = -normalize(viewSpacePosition);
    // world space view direction
    vec3 wo = (inverse(viewParameters.viewMatrix) * vec4(viewDirection, 0.f)).xyz;

    // 5m sample radius
    const float worldSpaceSampleRadius = 5.f;
    const float nearClippingPlane = .1f;
    // convert world space sample radius to screen space
    float screenSpaceEffectRadius = min(worldSpaceSampleRadius / abs(viewSpacePosition.z) * .5f, .05f);

    // randomly offset the sample positions along a slice
	float offset = texture(blueNoise_1024x1024_RGBA, gl_FragCoord.xy / float(textureSize(blueNoise_1024x1024_RGBA, 0).xy)).r;
    screenSpaceEffectRadius *= offset * 0.5f + 0.5f;
    float stepSize = screenSpaceEffectRadius / numSamplesPerDirection;

    float visibility = 0.f;

    float rotation = texture(blueNoise_16x16_R[viewParameters.frameCount % 8], gl_FragCoord.xy / textureSize(blueNoise_16x16_R[0], 0)).r * 2.f * PI;
	mat2 rotMat2 = {
		{  cos(rotation), sin(rotation) },
		{ -sin(rotation), cos(rotation) }
	};
    vec2 dir = rotMat2 * vec2(1.f, 0.f);

    vec3 cameraRightVector = viewParameters.cameraRight;
    vec3 cameraUpVector = viewParameters.cameraUp;
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

	for (int j = 0; j < numSamplesPerDirection; ++j)
	{
		vec2 sampleCoord = pixelCoord + (j * stepSize) * dir;

		// calculate theta1 
		float sz1 = texture(sceneDepth, sampleCoord).r;
		vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
		vec3 sx1 = s1 - x;

        // todo: maybe should use unattenuated horizon angle for indirect irradiance calculation while use attenuated for ssao ..?
        // todo: maybe instead of attenuate to PI, should only attenuate it to normal hemisphere ...?
		// attenuate this sample's horizon angle based on distance
		float t1 = mix(-acos(dot(normalize(sx1), wo)), -PI, clamp(length(sx1) / worldSpaceSampleRadius, 0.f, 1.f));  // t stands for theta
		h1 = max(h1, t1);

		// calculate theta2
		sampleCoord = pixelCoord - (j * stepSize) * dir;
		float sz2 = texture(sceneDepth, sampleCoord).r;
		vec3 s2 = screenToWorld(vec3(sampleCoord, sz2) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
		vec3 sx2 = s2 - x;

		// attenuate this sample's horizon angle based on distance
		float t2 = mix(acos(dot(normalize(sx2), wo)), PI, clamp(length(sx2) / worldSpaceSampleRadius, 0.f, 1.f));
		h2 = min(h2, t2);
	}

	// calculate inner integral analytically
	float a = 0.25f * (-cos(2.f * h1 - gamma) + cos(gamma) + 2.f * h1 * sin(gamma)) + .25f * (-cos(2.f * h2 - gamma) + cos(gamma) + 2.f * h2 * sin(gamma));
    // integrate slices numerically
	visibility += a * length(np);

    outAO = vec3(visibility);

    #if 0
    // temporal filtering
    // todo: take pixel velocity into consideration when doing reprojection
    // todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
    // todo: adaptive convergence-aware spatial filtering
    if (frameCount > 0)
    {
        vec3 prevViewSpacePos = (prevFrameView * vec4(x, 1.f)).xyz;
		vec4 prevNDCPos = prevFrameProjection * vec4(prevViewSpacePos, 1.f);
        prevNDCPos /= prevNDCPos.w;
        prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

        if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
        {
			float prevFrameDeviceZ = texture(prevFrameSceneDepthTexture, prevNDCPos.xy).r;
            vec3 cachedPrevFrameViewSpacePos = (prevFrameView * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameView), inverse(prevFrameProjection)), 1.f)).xyz;
            float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;

            float smoothing = .9f;
			smoothing = clamp(smoothing * 0.05f / relativeDepthDelta, 0.f, smoothing);
            // based on how fast the camera/object moves to further refine the smoothing, maybe it's better to use screen space displacement instead
            vec3 cameraDisplacement = view[3].xyz - prevFrameView[3].xyz;
            smoothing = mix(smoothing, 0.f, clamp(length(cameraDisplacement) / 0.1f, 0.f, 1.f));

			float AOHistory = texture(AOHistoryBuffer, prevNDCPos.xy).r;
			outAO = vec3(AOHistory * smoothing + visibility * (1.f - smoothing));
		}
	}
    #endif
}
