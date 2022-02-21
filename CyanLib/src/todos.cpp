/*
    Papers reading list
    * practical sky light model
    * irradiance caching
    * radiance caching
    * Practical voxel based GI (the original sparse octree voxel cone tracing and Nvidia's VXGI)
    * DDGI
*/

/*
    @Refactoring
    * refactor Mesh class and mesh loading code path, and Line class
    * material rework
    * add shader #include to reduce duplicated code in shader
    * 

    @Precompute GI 
    * add octree for irradiance cache
    * multi-threading for path tracing using irradiance caching
    * refactor light mapper using irradiance cache to bake lightmap
    * try to scale the lightmapper to bake sponza...?

    @Runtime dynamic GI
    * voxel cone tracing
    * surfel based GI (GIBS)
    * irradiance volume
    * DDGI
    
    @Runtime lighting
    * area light
    * material layering
*/

/*
    Bugs
    * mouse picking gltf-2.0 entities doesn't work
    * mouse picking issues
*/

/*
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
        * denoise..?
        * irradiance caching plus photon mapping..? (ue4)
    * shading model
        * roughness < .1f will cause specular highlight from sun light to disappear
    * animation
*/