#version 450 core
in vec2 texCoords;
out vec4 fragColor;
uniform vec3 cameraPos;
uniform mat4 view;
uniform mat4 projection;
// use interpolated fragment normal for the first pass
uniform sampler2D normalTexture; 
uniform sampler2D depthTexture;
vec3 screenToWorld(vec3 pp, mat4 invView, mat4 invProjection) {
    vec4 p = invProjection * vec4(pp, 1.f);
    p /= p.w;
    p = invView * p;
    return p.xyz;
}
// referrence: the GTAO paper (https://iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf)
void main()
{
    float ao = 0.f;
    float t1 = 0.f, t2 = 0.f, r = 0.f;
    // do we need to disble the bilinear filtering to get the exact pxiel depth?
    float depth = texture(depthTexture, texCoords);
    vec2 uv = 2.0f * texCoords - vec2(1.f);
    vec3 pp = vec3(uv, depth);
    mat4 invProj = inverse(projection);
    // pixel world pos
    vec3 x = screenToWorld(pp, invProj); 
    vec3 wo = normalize(pw - cameraPos);
    vec2 texelOffset = vec2(1.f) / imageSize(depthTexture);
    vec2 dir = vec2(0.5, 0.5);
    for (int i = 0; i < 2; ++i)
    {
        // compute horizon angle theta1
        vec2 suv1 = dir * texelOffset * i + texCoords;
        float sz1 = texture(depthTexture, suv);
        vec3 ss1 = vec3(suv, sz);
        vec3 s1 = screenToWorld(ss);
        vec3 sx1 = normalize(s - x);
        t1 = max(t1, acos(dot(sx1, wo)));

        // compute horizon angle theta2
        vec2 suv2 = -dir * texelOffset * i + texCoords;
        float sz2 = texture(depthTexture, suv2);
        vec3 ss2 = vec3(suv2, sz2);
        vec3 s2 = screenToWorld(ss2);
        vec3 sx2 = normalize(s2 - x);
        t2 = max(t2, acos(dot(sx2, wo)));

        // compute gamma (need face normal)
        /*
            * Note(Min): The ao coefficients A is described by cos(theta - gamma). Imagine that
            we are integrating ao for one azimuthal slice of the sphere by sampling points 
            on the unoccluded arc of the azimuthal slice. theta is the angle between 
            the view vector wo and the direction formed by connecting a vector
            from pixel's world position x to current integration sample's position. The bigger the theta,
            our integration sample is more close to the horizon theta1 (getting closer to the occlusing horizon),
            thus ao will decrease to indicate that less lights gets to x. When integration sample theta is aligned with
            the normal cos(theta - gamma) = cos(0) = 1, x receives most light.
        */
        // solving inner integral analytically
        ao += 0.25f * (-cos(2.f * t1 - r) + cos(r) + 2.f * t1 * sin(r));
        ao += 0.25f * (-cos(2.f * t2 - r) + cos(r) + 2.f * t2 * sin(r));
    }
}