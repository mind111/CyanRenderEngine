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
vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p = invView * p;
    return p.xyz;
}
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
    float clipUv = dot(uv, vec2(1.f));
    if (clipUv >=0.f && clipUv <= 2.f)
    {
        return texture(depthTexture, uv).r;
    }
    return 1.0f;
}

float computeSampleRadius(float depth)
{
    float radiusMips[4] = { 160.f, 100.f, 80.f, 40.f };
    int mip = int(floor(depth * 4));
    return radiusMips[mip];
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

// referrence: the GTAO paper (https://iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf)
void main()
{
    // do we need to disble the bilinear filtering to get the exact pxiel depth?
    float depth = texture(depthTexture, texCoords).r;
    vec2 uv = 2.0f * texCoords - vec2(1.f);
     depth = depth * 2.f - 1.f;
    vec3 pp = vec3(uv, depth);
    mat4 invProj = inverse(projection);
    mat4 invView = inverse(view);
    vec2 texelOffset = vec2(1.f) / vec2(1280.f, 720.f);

    // pixel world pos
    vec3 x = screenToWorld(pp, invView, invProj); 

    // scale sampling radius based on distance to the camera
    float pixelRadius = computeSampleRadius((depth + 1.f) / 2.f);

    vec3 wo = normalize(cameraPos - x);

    // sample sphere radiu in world space
    float sampleRadius = 2.f;

    vec3 normal = texture(normalTexture, texCoords).rgb;
    normal = normal * 2.f - vec3(1.f);
    float totalAo = 0.f;
    // horizon angle t1
    float cosT1 = 0.f;
    // horizon angle t2
    float cosT2 = 0.f; 
    // angle between projected normal with wo
    float r = 0.f;
    float debugh1 = 0.f;
    float debugh2 = 0.f;
    float debugGamma = 0.f;
    float debugProjectedLen = 0.f;
    vec3 debugPn = vec3(0.f);

    float numSteps = 8.f;
    float stride = pixelRadius / numSteps;

    // integrating unocclused surface area on the hemisphere
    for (int s = 0; s < 8; ++s)
    {
        float ao = 0.f;
        cosT1 = -0.f;
        cosT2 = -0.f;
        vec3 sxDir = vec3(0.f);
        vec2 sampleDir = computeSampleDirection(s * 0.125f * pi);

        for (float ii = 1.f; ii < numSteps; ++ii)
        {
            // compute horizon angle theta1
            vec2 suv1 = sampleDir * texelOffset * ii * stride + texCoords;
            float sz1 = sampleDepthTexture(suv1);
            suv1 = suv1 * 2.f - vec2(1.f);
            sz1 = sz1 * 2.f - 1.f;

            vec3 ss1 = vec3(suv1, sz1);
            vec3 s1 = screenToWorld(ss1, invView, invProj);
            vec3 sx1 = s1 - x;
            float len1 = length(sx1);
            sx1 = normalize(sx1);
            sxDir = sx1;
            float cosTt1 = dot(sx1, wo);
            if (cosTt1 > cosT1)
            {
                float atten = clamp((len1 / sampleRadius), 0.f, 1.f);
                cosT1 = mix(cosTt1, cosT1, atten);
            }

            // compute horizon angle theta2
            vec2 suv2 = -sampleDir * texelOffset * ii * stride + texCoords;
            float sz2 = sampleDepthTexture(suv2);
            suv2 = suv2 * 2.f - vec2(1.f);
            sz2 = sz2 * 2.f - 1.f;

            vec3 ss2 = vec3(suv2, sz2);
            vec3 s2 = screenToWorld(ss2, invView, invProj);
            vec3 sx2 = s2 - x;
            float len2 = length(sx2);
            sx2 = normalize(sx2);
            float cosTt2 = dot(sx2, wo);
            if (cosTt2 > cosT2)
            {
                float atten = clamp((len2 / sampleRadius), 0.f, 1.f);
                cosT2 = mix(cosTt2, cosT2, atten);
            }
        }

        // projected normal on wo, sx slice
        vec3 pn = normalize(cross(wo, sxDir));
        vec3 ny = pn * dot(pn, normal);
        vec3 nx = normalize(normal - ny);
        r = acos(clamp(dot(nx, wo), -1.f, 1.f));

        // h1 < 0
        float t1 = acos(clamp(cosT1, -1.f, 1.0f));
        // h2 > 0
        float t2 = -acos(clamp(cosT2, -1.f, 1.0f));

        // clamp to normal hemisphere
        float h1 = (r + min(t1 - r, .5 * pi));
        float h2 = (r + max(t2 - r, -.5 * pi));
        debugh1 = h1;
        debugh2 = h2;
        debugGamma = r;

        ao += 0.25f * (-cos(2.f * h1 - r) + cos(r) + 2.f * h1 * sin(r));
        ao += 0.25f * (-cos(2.f * h2 - r) + cos(r) + 2.f * h2 * sin(r));

        // TODO: figure out why this product darkens ao a lot
        // ao *= length(normal - ny);
        debugProjectedLen = length(normal - ny);
        totalAo += ao;
    }
    totalAo /= 8.f;

    // debugging scale by projected normal's length
    vec3 debugWo = vec3(0.f, 0.f, 1.f);

    float debugAo = 0.25f * (-cos(2.f * debugh1 - debugGamma) + cos(debugGamma) + 2.f * debugh1 * sin(debugGamma));
    debugAo += 0.25f * (-cos(2.f * debugh2 - debugGamma) + cos(debugGamma) + 2.f * debugh2 * sin(debugGamma));
    debugAo *= debugProjectedLen;
    debugPn = debugPn * .5f + vec3(.5f);
    fragColor = vec4(vec3(totalAo), 1.f);
}