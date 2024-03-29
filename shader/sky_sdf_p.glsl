#version 450 core

in vec3 fragmentObjPos;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec3 fragNormal;
layout (location = 2) out vec3 fragDepth;
layout (location = 3) out vec3 radialDepth;

#define pi 3.14159265359

struct Hit {
    float t;
    int objectId;
};

struct TraceInfo {
    float d;
    int objectId;
};

// object id #0
float sdTerrain(vec3 p, float height) {
    return p.y - height;
}

// object id #1
float sdSphere(vec3 p, vec3 c, float r) {
    return length(p - c) - r; 
}

TraceInfo scene(vec3 p) {
    float dSphere = sdSphere(p, vec3(0.f, 1.f, -3.f), 0.2f);
    float dTerrain = sdTerrain(p, -0.01f);
    int objectId = -1;
    if (dSphere < dTerrain) {
        objectId = 1; 
    } else {
        objectId = 0;
    }
    return TraceInfo(min(dSphere, dTerrain), objectId);
}

Hit castRay(vec3 ro, vec3 rd) {
    float t = 0.f;
    const uint MAX_ITER = 100u;
    const float maxT = 100.f;
    const float EPISILON = 0.001f; 
    for (uint iter = 0u; iter < MAX_ITER; ++iter) {
        TraceInfo trace = scene(ro + t * rd);
        t += trace.d;
        if (t > maxT) {
            break;
        }
        // hit
        if (trace.d < EPISILON) {
            return Hit(t, trace.objectId);
        }
    }
    return Hit(-1.f, -1);
}

// procedural sdf rendering of a sky and a terrain
void main() {
    vec3 ro = vec3(0.f);
    vec3 rd = normalize(fragmentObjPos);

    // default background color
    vec3 horizonColor = vec3(1.f, 0.279f, 0.1f);
    //vec3 skyDomeColor = vec3(0.200, 0.449, 1.0000);
    vec3 skyDomeColor = vec3(0.128f, 0.207f, 1.f);
    float k = smoothstep(-0.18, 0.01, rd.y);
    vec3 skyColor = mix(horizonColor, skyDomeColor, k);
    fragColor = vec4(skyColor, 1.f);
    vec3 colorA = vec3(.9f);
    vec3 colorB = vec3(.2f);

    // ray marching
    Hit hit = castRay(vec3(0.f), rd);
#if 0
    if (hit.t > 0.f) {
        vec3 p = ro + hit.t * rd;
        if (hit.objectId == 0) {
            fragColor = (.5 + .5 * rd.y) * vec4(0.95f, 0.60f, 0.30f, 1.f);
        }
    }
#endif

    fragNormal = vec3(0.f);
    fragDepth = vec3(1.f);
    // TODO: hard-coded for now
    radialDepth = vec3(1.f);
}
