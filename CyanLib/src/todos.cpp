/*
    Papers reading list
    * practical sky light model
    * irradiance caching
    * radiance caching
    * Cook's survey paper on sampling techniques
*/

/*
    * gltf-2.0 assets loading (sort of done)
    * saving the scene and assets as binaries (serialization)
    * a simple material editor
    * irradiance volume
    * local reflection probes
        * reflection doesn't have shadow
    * screen-space ray tracing
        * or world space .. using gpu?
    * shadow improvements
        * instead of using box filter, maybe use randonly rotated poisson samples? 
        * pcf 5x5 shadow acne issue
        * reducing the far clipping plane for view frustum almost fixed everything, but this is only a work-around for now.
        * debug visualize shadow cascades add blending between cascades.
    * lightmapping improvements
        * super-sampling each lightmap texel
        * is backface culling necessary?
        * only one bounce but exaggerate indirect bounce contribution
        * denoise..? 
        * irradiance caching plus photon mapping..? (ue4)
    * physically based lighting units
    * practical sky rendering
    * animation
    * shading model
        * roughness < .1f will cause specular highlight from sun light to disappear
*/