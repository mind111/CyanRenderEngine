#pragma once

#include "Scene.h"
#include "CyanRenderer.h"
#include "BVH.h"

namespace Cyan
{
    // SoA data oriented triangle mesh meant for improving ray tracing procedure
    struct TriangleArray 
    {
        u32 numVerts() const { return positions.size(); }
        u32 numTriangles() const { return positions.size() / 3; }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> texCoords;
    };

    struct SurfaceArray : public TriangleArray
    {
        std::vector<u32> materials;
    };

    struct RayTracingMesh
    {
        std::vector<TriangleArray> submeshes;
    };

    struct RayTracingMeshInstance
    {
        glm::mat4 worldTransform;
        // index into the global mesh array managed by the parent scene
        u32 parent;
        // index into the global material array managed by the parent scene
        std::vector<u32> materials;
    };

    // todo: support texture maps at some point ...?
    struct RayTracingMaterial
    {
        glm::vec3 albedo = glm::vec3(.94f);
        glm::vec3 emissive = glm::vec3(1.f);
        f32 roughness = 0.8f;
        f32 metallic = 0.1f;
    };

    // above struct with correct padding
    struct GPURayTracingMaterial
    {
        glm::vec4 albedo;
        glm::vec4 emissive;
        glm::vec4 roughness;
        glm::vec4 metallic;
    };

    struct RayTracingScene
    {
        /**
        * Convert a general Cyan::Scene to Cyan::RayTracingScene
        */
        RayTracingScene(const Scene& scene);

        PerspectiveCamera camera;

        std::vector<RayTracingMesh> meshes;
        std::vector<RayTracingMaterial> materials;
        std::vector<RayTracingMeshInstance> meshInstances;

        // flattened big triangle array used to perform the actual tracing
        SurfaceArray surfaces;

        ShaderStorageBuffer<DynamicSsboData<glm::vec4>> positionBuffer;
        ShaderStorageBuffer<DynamicSsboData<glm::vec4>> normalBuffer;
        ShaderStorageBuffer<DynamicSsboData<GPURayTracingMaterial>> materialBuffer;

        // todo: lights
    };
}
