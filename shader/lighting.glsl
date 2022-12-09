#version 450 core

vec3 calcDirectionalLight(in DirectionalLight directionalLight, MaterialParameters materialParameters, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);
    // view direction in world space
    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.f)).xyz;
    float ndotl = max(dot(materialParameters.normal, directionalLight.direction.xyz), 0.f);
    vec3 f0 = calcF0(materialParameters);
    vec3 li = directionalLight.colorAndIntensity.rgb * directionalLight.colorAndIntensity.a;

    /** 
    * diffuse
    */
    /*
    vec3 h = normalize(directionalLight.direction.xyz + worldSpaceViewDirection);
    float hdotl = saturate(dot(h, directionalLight.direction.xyz));
    float ndotv = saturate(dot(materialParameters.normal, worldSpaceViewDirection));
    vec3 diffuse = mix((1.f - f0), vec3(0.f), materialParameters.metallic) * DisneyDiffuseBRDF(hdotl, ndotl, ndotv, materialParameters);
    */
    vec3 diffuse = mix(materialParameters.albedo, vec3(0.f), materialParameters.metallic) * LambertBRDF();

    /** 
    * specular
    */
    vec3 specular = CookTorranceBRDF(directionalLight.direction.xyz, worldSpaceViewDirection, materialParameters.normal, materialParameters.roughness, f0);

    radiance += (diffuse + specular) * li * ndotl;

    // shadow
    radiance *= calcDirectionalShadow(worldSpacePosition, materialParameters.normal, directionalLight);
    return radiance;
}

vec3 calcPointLight()
{
    vec3 radiance = vec3(0.f);
    return radiance;
}

vec3 calcSkyLight(SkyLight inSkyLight, in MaterialParameters materialParameters, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);

    vec3 worldSpaceViewDirection = (inverse(viewSsbo.view) * vec4(normalize(-psIn.viewSpacePosition), 0.0)).xyz;
    float ndotv = saturate(dot(materialParameters.normal, worldSpaceViewDirection));

    vec3 f0 = calcF0(materialParameters);

    float ao = 1.f;

    // irradiance
    vec3 f = fresnel(f0, materialParameters.normal, worldSpaceViewDirection);
    vec3 diffuse = mix(materialParameters.albedo, vec3(0.f), materialParameters.metallic);
    vec3 irradiance = diffuse * texture(skyLight.irradiance, materialParameters.normal).rgb; 
    radiance += irradiance * ao;

    // reflection
    vec3 reflectionDirection = -reflect(worldSpaceViewDirection, materialParameters.normal);
    vec3 BRDF = texture(sampler2D(inSkyLight.BRDFLookupTexture), vec2(ndotv, materialParameters.roughness)).rgb; 
    vec3 incidentRadiance = textureLod(samplerCube(skyLight.reflection), reflectionDirection, materialParameters.roughness * 10.f).rgb;
    radiance += incidentRadiance * (f0 * BRDF.r + BRDF.g) * ao;

    return vec3(0.f);
}

vec3 calcLighting(in MaterialParameters materialParameters, vec3 worldSpacePosition) {
    vec3 radiance = vec3(0.f);

    // sun light
    radiance += calcDirectionalLight(directionalLights[0], materialParameters, worldSpacePosition);
    // sky light
    radiance += calcSkyLight(skyLight, materialParameters, worldSpacePosition);

    return radiance;
}
