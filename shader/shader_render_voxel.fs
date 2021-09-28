#version 450 core

out vec4 fragColor;

in vec2 texCoords;

uniform vec3 cameraPos;
uniform vec3 cameraLookAt;

uniform sampler3D albedoMap;
uniform sampler3D normalMap;

// scene bounding box
uniform vec3 aabbMin;
uniform vec3 aabbMax;

// TODO: This is mostly working but need to take care of the case when the camera is out side of 
// the scene's bounding box and weird visual artifact when the camera is inside of an object (too close
// to something)
void main()
{
    vec4 bgColor = vec4(0.7f, 0.7f, 1.0f, 1.0f);
    vec3 ro = cameraPos;
    vec3 forward = normalize(cameraLookAt - cameraPos);
    // vec3 worldUp = abs(forward.x) > 0.f ? vec3(0.f, 1.f, 0.f) : vec3(1.f, 0.f, 0.f);
    vec3 worldUp = vec3(0.f, 1.f, 0.f);
    vec3 right = cross(forward, worldUp);
    vec3 up = cross(right, forward);
    vec2 uv = texCoords * 2.f - vec2(1.f);
    vec3 rd = normalize(uv.x * right + uv.y * up + forward * 80.0f);

    float near = aabbMax.z;
    float t = (near - ro.z) / rd.z;

    // stepSize too big leads holes in the visualization
    float stepSize = 0.01f;
    float numSteps = 2.0f * (aabbMax.z - aabbMin.z) / stepSize;
    vec4 color = vec4(0.f);
    // scene aabb related
    vec3 bottomLeft = vec3(aabbMin.x, aabbMin.y, aabbMax.z);
    vec3 topRight = vec3(aabbMax.x, aabbMax.y, aabbMin.z);
    // camera is within the scene's bounding box
    ro = ro + uv.x * right + uv.y * up;
    vec3 p = ro + 7.1f * rd;
    vec3 texCoord = (p - bottomLeft) / (topRight - bottomLeft);
    vec3 texColor = texture(albedoMap, vec3(texCoord.xyz)).rgb;

    for (float step = 0.f; step < numSteps && color.a < 0.99; ++step)
    {
        vec3 p = ro + step * stepSize * rd;
        vec3 texCoord = (p - bottomLeft) / (topRight - bottomLeft);
        vec4 texColor = texture(albedoMap, texCoord);
        color += (1.f - color.a) * texColor;
    }
    fragColor = color;
}