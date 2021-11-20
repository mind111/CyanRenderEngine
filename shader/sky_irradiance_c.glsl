#version 450
// input
layout(local_size_x = 32, local_size_y = 32) in;
uniform samplerCube g_skybox;
struct Ray
{
    vec4 ro;
    vec3 rd;
    float visibility;
};
layout(std430, binding = 0) buffer RayData
{
    Ray rays[];
} rayBuffer;

// output
shared vec3 radiance[32 * 32];
shared vec3 totalRadiance;
layout(binding = 0, rgba16f) uniform image3D irradianceMap;

/* 
    procedure to convert a direction vector into texcoord to sample a cubemap according to 
    OpenGL spec 4.5
*/
vec3 directionToTexcoord(vec3 rd)
{
    vec3 temp = vec3(0.f);
    float ma = abs(rd.x) > abs(rd.y) ? rd.x : rd.y;
    ma = abs(rd.z) >= abs(ma) ? rd.z : ma;
    if (ma == rd.x)
    {
        if (ma > 0.f)
            temp = vec3(-rd.z, -rd.y, 0.f); else
            temp = vec3(rd.z, -rd.y, 1.f);
    }
    else if(ma == rd.y)
    {
        if (ma > 0.f)
            temp = vec3(rd.x, -rd.z, 2.f); 
        else
            temp = vec3(rd.x, -rd.z, 3.f);
    }
    else
    {
        if (ma > 0.f)
            temp = vec3(rd.x, -rd.y, 4.f); 
        else
            temp = vec3(-rd.x, -rd.y, 5.f);
    }
    float s = .5f * (temp.x / abs(ma) + 1.f);
    float t = .5f * (temp.y / abs(ma) + 1.f);
    return vec3(s, t, temp.z);
}

void main()
{
    uint localThreadIndex = gl_LocalInvocationIndex;
    // initialize shared totalRadiance
    radiance[localThreadIndex] = vec3(0.f);

    barrier();

    uint rayIndex = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x + gl_GlobalInvocationID.x;
    Ray ray = rayBuffer.rays[rayIndex];
    vec3 texCoord = directionToTexcoord(ray.rd);
    if (ray.visibility > 0.f)
        radiance[localThreadIndex] = texture(g_skybox, ray.rd).rgb;

    // sync with other threads in the workgroup
    barrier();
    memoryBarrierShared();

    if (localThreadIndex == 0)
    {
        int numSampleRays = 32 * 32;
        vec3 irradiance = vec3(0.f);
        // TODO: this is slow, optimize!
        for (int i = 0; i < numSampleRays; ++i)
        {
            // TODO: cosine weighted
            irradiance += radiance[i];
        }
        irradiance /= numSampleRays;

        ivec3 pixelCoord = ivec3(texCoord.xy * imageSize(irradianceMap).xy, int(texCoord.z));
        imageStore(irradianceMap, pixelCoord, vec4(irradiance, 1.f));
    }
}