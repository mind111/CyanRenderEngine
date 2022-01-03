/*
    Papers reading list
    * practical sky light model
    * irradiance caching
    * radiance caching
    * Cook's survey paper on sampling techniques
    * Practical voxel based GI (the original sparse octree voxel cone tracing and Nvidia's VXGI)
*/

/*
    Bugs
    * mouse picking gltf-2.0 entities doesn't work
    * mouse picking issues
*/

/*
    * try to load the classic Sponza scene, and make the rendering work.
    * integrate hosek skylight model && study about using photometric lighting units different type of light sources
        * physically based lighting units
        * practical sky rendering
    * irradiance caching
    * irradiance gradients
    * irradiance volume
    * fix GTAO
    * gltf-2.0 assets loading (sort of done)
    * saving the scene and assets as binaries (serialization)
    * a simple material editor
    * local reflection probes
        * reflection doesn't have shadow
        * no sky backgroud reflection
    * screen-space ray tracing
        * or world space .. using gpu?
    * shadow improvements
        * instead of using box filter, maybe use randonly rotated poisson samples? 
        * pcf 5x5 shadow acne issue
        * reducing the far clipping plane for view frustum almost fixed everything, but this is only a work-around for now.
        * debug visualize shadow cascades add blending between cascades.
    * lightmapping improvements
        * bake static ao using path tracer, and disble ssao for static geometry
        * is backface culling necessary?
        * denoise..?
        * irradiance caching plus photon mapping..? (ue4)
    * animation
    * shading model
        * roughness < .1f will cause specular highlight from sun light to disappear
*/