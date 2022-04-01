#version 450 core

out VS_OUT
{
    vec3 p;
    vec3 dir;
} vsOut;

layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

uniform sampler2D sceneDepthTexture;
uniform sampler2D sceneNormalTexture;

uniform vec2 debugScreenPos;

const vec3 debugDirections[5] = { 
    vec3(0.f, 0.f, 1.f),
    vec3(1.7f, 0.f, 0.5f),
	vec3(-1.7f, 0.f, 0.5f),
    vec3(0.f, 1.7f, 0.5f),
    vec3(0.f, -1.7f, 0.5f)
};

mat3 tbn(vec3 n)
{
    vec3 up = abs(n.y) < 0.98f ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
    vec3 right = cross(n, up);
    vec3 forward = cross(n, right);
    return mat3(
        right,
        forward,
        n
    );
}

vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p.w = 1.f;
    
    p = invView * p;
    return p.xyz;
}

void main()
{
    vec2 texCoord = debugScreenPos * .5f + .5f;
    float depth = texture(sceneDepthTexture, texCoord).r;
    vec3 debugWorldPos = screenToWorld(vec3(debugScreenPos, depth * 2.f - 1.f), inverse(gDrawData.view), inverse(gDrawData.projection));
    vec3 n = normalize(texture(sceneNormalTexture, texCoord).rgb * 2.f - 1.f);

    vsOut.p = debugWorldPos;
    vsOut.dir = tbn(n) * normalize(debugDirections[gl_VertexID]);
    gl_Position = vec4(debugWorldPos, 1.f);
}
