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
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;

uniform sampler2D u_sceneDepthTex;
uniform sampler2D u_prevFrameSceneDepthTex;
uniform sampler2D u_sceneNormalTex;
// todo: maybe using the same noises from the GTAO talk will produce better results?
uniform sampler2D u_blueNoiseTex_16x16_R[8];
uniform sampler2D u_blueNoiseTex_1024x1024_RGBA;
//---------------------------------------------------------------------------------
uniform sampler2D u_AOHistoryTex;

out vec3 outAO;

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
    float xz = texture(u_sceneDepthTex, pixelCoord).r;
    vec3 n = texture(u_sceneNormalTex, pixelCoord).xyz * 2.f - 1.f;

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
	float offset = texture(u_blueNoiseTex_1024x1024_RGBA, gl_FragCoord.xy / float(textureSize(u_blueNoiseTex_1024x1024_RGBA, 0).xy)).r;
    screenSpaceEffectRadius *= offset * 0.5f + 0.5f;
    float stepSize = screenSpaceEffectRadius / numSamplesPerDirection;

    float visibility = 0.f;

    float rotation = texture(u_blueNoiseTex_16x16_R[viewParameters.frameCount % 8], gl_FragCoord.xy / textureSize(u_blueNoiseTex_16x16_R[0], 0)).r * 2.f * PI;
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
		float sz1 = texture(u_sceneDepthTex, sampleCoord).r;
		vec3 s1 = screenToWorld(vec3(sampleCoord, sz1) * 2.f - 1.f, inverse(viewParameters.viewMatrix), inverse(viewParameters.projectionMatrix));
		vec3 sx1 = s1 - x;

        // todo: maybe should use unattenuated horizon angle for indirect irradiance calculation while use attenuated for ssao ..?
        // todo: maybe instead of attenuate to PI, should only attenuate it to normal hemisphere ...?
		// attenuate this sample's horizon angle based on distance
		float t1 = mix(-acos(dot(normalize(sx1), wo)), -PI, clamp(length(sx1) / worldSpaceSampleRadius, 0.f, 1.f));  // t stands for theta
		h1 = max(h1, t1);

		// calculate theta2
		sampleCoord = pixelCoord - (j * stepSize) * dir;
		float sz2 = texture(u_sceneDepthTex, sampleCoord).r;
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

    // temporal filtering
    // todo: take pixel velocity into consideration when doing reprojection
    // todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
    // todo: adaptive convergence-aware spatial filtering
    if (viewParameters.frameCount > 0)
    {
        mat4 prevFrameViewMatrix = viewParameters.prevFrameViewMatrix;
        mat4 prevFrameProjectionMatrix = viewParameters.prevFrameProjectionMatrix;

        vec3 prevViewSpacePos = (prevFrameViewMatrix * vec4(x, 1.f)).xyz;
		vec4 prevNDCPos = prevFrameProjectionMatrix * vec4(prevViewSpacePos, 1.f);
        prevNDCPos /= prevNDCPos.w;
        prevNDCPos.xyz = prevNDCPos.xyz * .5f + .5f;

        if (prevNDCPos.x <= 1.f && prevNDCPos.x >= 0.f && prevNDCPos.y <= 1.f && prevNDCPos.y >= 0.f)
        {
			float prevFrameDeviceZ = texture(u_prevFrameSceneDepthTex, prevNDCPos.xy).r;
            vec3 cachedPrevFrameViewSpacePos = (prevFrameViewMatrix * vec4(screenToWorld(vec3(prevNDCPos.xy, prevFrameDeviceZ) * 2.f - 1.f, inverse(prevFrameViewMatrix), inverse(prevFrameProjectionMatrix)), 1.f)).xyz;
            float relativeDepthDelta = abs(cachedPrevFrameViewSpacePos.z - prevViewSpacePos.z) / -cachedPrevFrameViewSpacePos.z;

            float smoothing = .9f;
			smoothing = clamp(smoothing * 0.05f / relativeDepthDelta, 0.f, smoothing);
            // based on how fast the camera/object moves to further refine the smoothing, maybe it's better to use screen space displacement instead
            vec3 cameraDisplacement = viewParameters.cameraPosition - viewParameters.prevFrameCameraPosition;
            smoothing = mix(smoothing, 0.f, clamp(length(cameraDisplacement) / 0.1f, 0.f, 1.f));

			float AOHistory = texture(u_AOHistoryTex, prevNDCPos.xy).r;
			outAO = vec3(AOHistory * smoothing + visibility * (1.f - smoothing));
		}
	}
}