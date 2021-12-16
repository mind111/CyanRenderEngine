#version 450 core
in vec2 texCoords;
out vec4 fragColor;
uniform vec3 cameraPos;
uniform mat4 view;
uniform mat4 projection;
#define pi 3.14159265359
// use interpolated fragment normal for the first pass
uniform sampler2D normalTexture; 
uniform sampler2D depthTexture;

layout(std430, binding = 0) buffer DebugVisData
{
    vec4 origin;
    vec4 normal;
    vec4 wo;
    vec4 sliceDir;
    vec4 projectedNormal;
    vec4 sampleVec[16];
    vec4 samplePoints[48];
    int numSampleLines;
    int numSamplePoints;
} debugVis;

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    p = invView * p;
    return p.xyz;
}

vec3 screenToView(vec3 pp, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    return p.xyz;
}

float hash(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

// TODO: add random jitter to trade banding artifact for noise
vec2 computeSampleDirection(float theta)
{
    mat2 rot = { 
        { cos(theta), sin(theta) },
        {-sin(theta), cos(theta) }
    };
    return rot * vec2(1.0f, 0.f);
}

float sampleDepthTexture(vec2 uv)
{
    uv.x = clamp(uv.x, 0.f, 1.f);
    uv.y = clamp(uv.y, 0.f, 1.f);
    return texture(depthTexture, uv).r;
}

vec2 computeSliceHorizons(float phi)
{
    vec2 sampleDir = computeSampleDirection(phi);
    return sampleDir;
}

float integrateArc()
{
    return 0.f;
}

// TODO: how can I debug this ...?
// TODO: debug visualize sample directions, and sample positions
// TODO: debug visualize horizon angle vectors
// TODO: maybe try switch to view space....(should be same as doing all the computation in world space)?

void drawDebugVis(vec3 sampleWorldPos)
{
    debugVis.origin = vec4(sampleWorldPos, 1.f);

    mat4 invProj = inverse(projection);
    mat4 invView = inverse(view);

    vec2 debugUv = vec2(0.5f, 0.2f);
    float depth = texture(depthTexture, debugUv).r;
    vec3 x = screenToWorld(vec3(debugUv * 2.f - 1.f, depth * 2.f -1.f), invView, invProj);
    debugVis.origin = vec4(x, 1.f);

    // find screen space pos of sample
    vec2 sampleScreenCoord = debugUv;
    // vec2 texelSize = vec2(1.f) / textureSize(depthTexture, 0); 
    vec2 texelSize = vec2(1.f) / vec2(1280.f, 720.f);
    float sampleDepth = texture(depthTexture, sampleScreenCoord).r;

    float maxRadiusInPixels = 200.f;
    float minRadiusInPixels = 16.f;
    float radiusInPixels = mix(maxRadiusInPixels, minRadiusInPixels, sampleDepth);
    float numSteps = 4.f;
    float numSlices = 8.f;
    // float stride = radiusInPixels / numSteps;
    // float stride = 30.f;

    float effectRadius = 1.0f;
    float zn = .5f;
    float radius = effectRadius * zn / abs(x.z - cameraPos.z);
    float screenWidth = (zn * 1.f) * 2.f;
    float screenSpaceRadius = radius / screenWidth;
    float stride = screenSpaceRadius / numSteps;

    // vec3 wo = normalize(vec3(0.f, 1.5f, -1.5f));
    vec3 wo = normalize(vec3(0.f, 0.1f, -2.f));
    vec3 normal = texture(normalTexture, sampleScreenCoord).rgb * 2.f - 1.f;
    debugVis.normal = vec4(normal, 0.f);
    debugVis.numSampleLines = 2;
    debugVis.numSamplePoints = 6;

    float totalAo = 0.f;
    for (int i = 0; i < numSlices; ++i)
    { 
        float r = 0.f;
        float ao = 0.f;
        float cosT1 = -1.f;
        float cosT2 = -1.f;
        float randomRotation = hash(i * sampleScreenCoord);
        vec2 sampleDir = computeSampleDirection(i * (pi / numSlices));
        vec3 sxDir = vec3(0.f);

        for (float ii = 1.f; ii < numSteps; ++ii)
        {
            int samplePointIndex = i * (int(numSteps) - 1) * 2 + int(ii - 1) * 2;
            float randomBias = 1.f + 0.3f * hash(sampleScreenCoord * vec2(ii, float(i)));

            // compute horizon angle theta1
            vec2 suv1 = sampleDir * ii * stride + sampleScreenCoord;
            float sz1 = sampleDepthTexture(suv1);
            suv1 = suv1 * 2.f - 1.f;
            sz1 = sz1 * 2.f - 1.f;

            vec3 ss1 = vec3(suv1, sz1);
            vec3 s1 = screenToWorld(ss1, invView, invProj);
            debugVis.samplePoints[samplePointIndex++] = vec4(s1, 1.f);
            vec3 sx1 = s1 - x;
            float len1 = length(sx1);
            sx1 = normalize(sx1);
            sxDir = sx1;
            float cosTt1 = dot(sx1, wo);
            if (cosTt1 >= cosT1)
            {
                cosT1 = cosTt1;
                debugVis.sampleVec[i * 2] = vec4(s1, 1.f);
            }

            // compute horizon angle theta2
            vec2 suv2 = -sampleDir * ii * stride + sampleScreenCoord;
            float sz2 = sampleDepthTexture(suv2);
            suv2 = suv2 * 2.f - 1.f;
            sz2 = sz2 * 2.f - 1.f;

            vec3 ss2 = vec3(suv2, sz2);
            vec3 s2 = screenToWorld(ss2, invView, invProj);
            debugVis.samplePoints[samplePointIndex] = vec4(s2, 1.f);
            vec3 sx2 = s2 - x;
            float len2 = length(sx2);
            sx2 = normalize(sx2);
            float cosTt2 = dot(sx2, wo);
            if (cosTt2 >= cosT2)
            {
                cosT2 = cosTt2;
                debugVis.sampleVec[i * 2 + 1] = vec4(s2, 1.f);
            }
        }

        // projected normal on wo, sx slice
        vec3 slicePlaneY = normalize(cross(wo, sxDir));
        vec3 slicePlaneX = normalize(cross(slicePlaneY, wo));
        vec3 pn = slicePlaneY;
        vec3 ny = pn * dot(pn, normal);
        float projectedNormalLen = length(normal - ny);
        vec3 nx = (normal - ny) / projectedNormalLen;
        float sign = sign(dot(nx, slicePlaneX));
        r = sign * acos(clamp(dot(nx, wo), 0.f, 1.f));

        if ( i == 0)
        {
            debugVis.projectedNormal = vec4(nx, 0.f);
            debugVis.wo = vec4(wo, 0.f);
            debugVis.sliceDir = vec4(sxDir, 0.f);
        }

        // h1 > 0
        float t1 = acos(clamp(cosT1, -1.f, 1.0f));
        // h2 < 0
        float t2 = -acos(clamp(cosT2, -1.f, 1.0f));

        // clamp to normal hemisphere
        float h1 = r + min(t1 - r, .5f * pi);
        float h2 = r + max(t2 - r, -.5f * pi);

        ao += 0.25f * (-cos(2.f * h1 - r) + cos(r) + 2.f * h1 * sin(r));
        ao += 0.25f * (-cos(2.f * h2 - r) + cos(r) + 2.f * h2 * sin(r));
        ao *= projectedNormalLen;
        totalAo += ao;
    }
    totalAo /= numSlices;
    debugVis.normal = vec4(0.f, totalAo, 0.f, 1.f);
}

float computeVisibility(vec3 worldPos, vec3 viewDir, float phi)
{
    return 1.0f;
}

#define DRAW_DEBUG_VIS 0

// TODO: improve distribution of samples, introduce noise?
// TODO: use world space sphere radius to determine sample distance in screen space
// TODO: bilateral filter to clean up the noise?

// referrence: the GTAO paper (https://iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf)
void main()
{
#if DRAW_DEBUG_VIS
    vec3 debugWorldPos = vec3(0.f, -1.2f, 0.f);
    drawDebugVis(debugWorldPos);
#endif

    float depth = texture(depthTexture, texCoords).r;
    if (depth >= 0.99f)
    {
        fragColor = vec4(1.f);
        return;
    } 

    vec2 uv = 2.0f * texCoords - vec2(1.f);
    vec3 pp = vec3(uv, depth * 2.f - 1.f);
    mat4 invProj = inverse(projection);
    mat4 invView = inverse(view);
    vec2 texelOffset = vec2(1.f) / vec2(1280.f, 720.f);

    // pixel world pos
    vec3 x = screenToWorld(pp, invView, invProj); 
    vec3 wo = normalize(cameraPos - x);

    float maxDistance = 50.f;
    float maxRadiusInPixels = 20.f;
    float minRadiusInPixels = 10.f;
    float radiusInPixels = mix(maxRadiusInPixels, minRadiusInPixels, length(x - cameraPos) / maxDistance);
    // float radiusInPixels = mix(maxRadiusInPixels, minRadiusInPixels, depth);
    float numSteps = 8.f;
    float stride = radiusInPixels / numSteps;

// TODO: figure out how to do this
#if 0
    float effectRadius = 1.0f;
    float zn = .5f;
    float radius = effectRadius * zn / abs(x.z - cameraPos.z);
    float screenWidth = (zn * 1.f) * 2.f;
    float screenSpaceRadius = clamp(radius / screenWidth, 0.f, 0.25f);
    float stride = screenSpaceRadius / numSteps;
#endif

    // sample sphere radiu in world space
    float sampleRadius = 1.f;
    int numSlices = 4;

    vec3 normal = texture(normalTexture, texCoords).rgb;
    normal = normalize(normal * 2.f - vec3(1.f));
    float totalAo = 0.f;
    // horizon angle t1
    float cosT1 = 0.f;
    // horizon angle t2
    float cosT2 = 0.f; 
    // angle between projected normal with wo
    float r = 0.f;
    float randomRotation = hash(texCoords * vec2(5.f, 73.f));

    // integrating unocclused surface area on the hemisphere
    for (int s = 0; s < numSlices; ++s)
    {
        float ao = 0.f;
        vec3 sxDir = vec3(0.f);
        cosT1 = -1.f;
        cosT2 = -1.f;
        float phi = s * (pi / numSlices);
        vec2 omega = vec2(cos(phi), -sin(phi)) * 16.f;

        vec2 sampleDir = computeSampleDirection(s * (pi / float(numSlices)) + randomRotation * 0.15 * pi);

        // skip the center
        for (float ii = 1.f; ii < numSteps; ++ii)
        {
            float randomBias = 1.f + 0.4f * hash(texCoords * vec2(ii, float(s)));

            // compute horizon angle theta1
            vec2 suv1 = sampleDir * texelOffset * ii * stride * randomBias + texCoords;
            // vec2 suv1 = sampleDir * ii * vec2(stride, stride * (16.f / 9.f)) + texCoords;

            float sz1 = sampleDepthTexture(suv1);
            suv1 = suv1 * 2.f - 1.f;
            sz1 = sz1 * 2.f - 1.f;

            vec3 ss1 = vec3(suv1, sz1);
            vec3 s1 = screenToWorld(ss1, invView, invProj);
            vec3 sx1 = s1 - x;
            float len1 = length(sx1);
            sx1 = normalize(sx1);
            sxDir = sx1;
            float cosTt1 = dot(sx1, wo);
            cosTt1 = mix(cosTt1, -1.f, len1/sampleRadius);
            if (cosTt1 > cosT1)
            {
                cosT1 = cosTt1;
            }

            // compute horizon angle theta2
            vec2 suv2 = -sampleDir * texelOffset * ii * stride * randomBias + texCoords;
            // vec2 suv2 = -sampleDir * ii * vec2(stride, stride * (16.f / 9.f)) + texCoords;

            float sz2 = sampleDepthTexture(suv2);
            suv2 = suv2 * 2.f - 1.f;
            sz2 = sz2 * 2.f - 1.f;

            vec3 ss2 = vec3(suv2, sz2);
            vec3 s2 = screenToWorld(ss2, invView, invProj);
            vec3 sx2 = s2 - x;
            float len2 = length(sx2);
            sx2 = normalize(sx2);
            float cosTt2 = dot(sx2, wo);
            cosTt2 = mix(cosTt2, -1.f, len2/sampleRadius);
            if (cosTt2 > cosT2)
            {
                cosT2 = cosTt2;
            }
        }

        // projected normal on wo, sx slice
        vec3 slicePlaneY = normalize(cross(wo, sxDir));
        vec3 slicePlaneX = normalize(cross(slicePlaneY, wo));
        vec3 pn = slicePlaneY;
        vec3 ny = pn * dot(pn, normal);
        float projectedNormalLen = length(normal - ny);
        vec3 nx = (normal - ny) / projectedNormalLen;
        float sign = -sign(dot(nx, slicePlaneX));
        r = sign * acos(clamp(dot(nx, wo), 0.f, 1.f));

        // h1 < 0
        float t1 = -acos(clamp(cosT1, -1.f, 1.0f));
        // h2 > 0
        float t2 = acos(clamp(cosT2, -1.f, 1.0f));

        // clamp to normal hemisphere
        float h1 = r + max(t1 - r, -.5f * pi);
        float h2 = r + min(t2 - r,  .5f * pi);

        ao += 0.25f * (-cos(2.f * h1 - r) + cos(r) + 2.f * h1 * sin(r));
        ao += 0.25f * (-cos(2.f * h2 - r) + cos(r) + 2.f * h2 * sin(r));

        ao *= projectedNormalLen;
        totalAo += ao;
    }
    totalAo /= float(numSlices);
    totalAo = pow(totalAo, 3.f);
    fragColor = vec4(vec3(totalAo), 1.f);
}