/*
    Papers reading list
    * practical sky light model
    * shared caching
    * radiance caching
    * Practical voxel based GI (the original sparse octree voxel cone tracing and Nvidia's VXGI)
    * DDGI
*/

/*
    @Refactoring
    * add shader #include to reduce duplicated code in shader

    @Precompute GI 
    * multi-threading for path tracing using shared caching
    * refactor light mapper using shared cache to bake lightmap
    * try to scale the lightmapper to bake sponza...?

    @Runtime dynamic GI
    * voxel cone tracing
    * surfel based GI (GIBS)
    * shared volume
    * DDGI
    
    @Runtime lighting
    * area light
    * material layering
*/

/*
    * integrate hosek skylight model && study about using photometric lighting units different type of light sources
        * physically based lighting units
        * practical sky rendering
    * shared caching
    * shared gradients
    * shared volume
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
        * shared caching plus photon mapping..? (ue4)
    * shading model
        * roughness < .1f will cause specular highlight from sun light to disappear
    * animation
*/