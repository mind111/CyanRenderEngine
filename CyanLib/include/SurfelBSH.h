#pragma once

#include <functional>

#include <glm/glm.hpp>

#include "Common.h"
#include "Surfel.h"
#include "ShaderStorageBuffer.h"

namespace Cyan {
    class GfxContext;
    struct RenderTarget;
    struct GfxTexture2D;

    /** 
    * Surfel Bounding Sphere Hierarchy
    */
    struct SurfelBSH {
        SurfelBSH();
        ~SurfelBSH() { }

        struct Node {
            void build(const std::vector<Surfel>& surfels, u32 surfelIndex);
            void build(const std::vector<Node>& nodes, u32 leftChildIndex, u32 rightChildIndex);
            bool isLeaf() const { return (childs[0] < 0 && childs[1] < 0); }
            bool intersect(const glm::vec3& ro, const glm::vec3& rd, f32& t) const;

            i32 childs[2] = { -1, -1 };
            glm::vec3 center = glm::vec3(0.f);
            f32 radius = 0.f;
            glm::vec3 normal = glm::vec3(0.f);
            glm::vec3 albedo = glm::vec3(0.f);
            glm::vec3 radiance = glm::vec3(0.f);
            f32 coneAperture = 0.f;
            i32 surfelID = -1;
        };

        /** 
        *  this is pretty wasteful, need to 
        */
        struct GpuNode {
            glm::vec4 center;
            glm::vec4 normal;
            glm::vec4 albedo;
            glm::vec4 radiance;
            glm::vec4 coneAperture;
            glm::vec4 radius;
            glm::ivec4 childs;
        };

        struct RayHit {
            Node node = { };
            f32 t = FLT_MAX;
        };

        friend class MicroRenderingGI;

        virtual void build(const std::vector<Surfel>& inSurfels);
        GfxTexture2D* getVisualization() { return visualization; }
        bool castRay(const glm::vec3& ro, const glm::vec3& rd, RayHit& hit) const;

        Node* root = nullptr;
        std::vector<Surfel> surfels;
        std::vector<Node> nodes;
        ShaderStorageBuffer<DynamicSsboData<GpuNode>> gpuNodes;
        std::vector<Node> leafNodes;
    protected:
        void dfs(i32 nodeIndex, const std::function<void(Node&)>& callback);
        bool castRayInternal(const glm::vec3& ro, const glm::vec3& rd, const Node& node, RayHit& hit) const;
        virtual u32 getNumLevels() { return numLevels; }
        ShaderStorageBuffer<DynamicSsboData<Instance>> nodeInstanceBuffer;
        GfxTexture2D* visualization = nullptr;
        i32 activeVisLevel = 0;
        i32 numVisLevels = 1;
        bool bVisualizeBoundingSpheres = true;
        bool bVisualizeNodes = true;
        i32 numLevels = -1;
    private:
        u32 allocNode();
        virtual void buildInternal(i32 nodeIndex, std::vector<Surfel>& surfels, u32 start, u32 end);
        virtual void visualize(GfxContext* gfxc, RenderTarget* dstRenderTarget);
        i32 levelFirstTraversal(i32 startLevel, i32 numLevelsToTraverse, const std::function<void(i32 nodeIndex)>& callback = [](i32 nodeIndex) { });
    };

#if 0
    struct CompleteSurfelBSH {
        friend class MicroRenderingGI;

        virtual void build(const std::vector<Surfel>& surfels);
        i32 getNumLevels();

        BSHNode* root = nullptr;
        std::vector<Surfel> surfels;
        std::vector<BSHNode> nodes;

    private:
        ShaderStorageBuffer<DynamicSsboData<Instance>> nodeInstanceBuffer;
        i32 activeVisLevel = 0;
        i32 numVisLevels = 1;
        void buildInternal(i32 parent, std::vector<Surfel>& surfels, u32 start, u32 end);
        bool bVisualizeBoundingSpheres = true, bVisualizeNodes = true;
        void visualize(GfxContext* gfxc, RenderTarget* dstRenderTarget);
        void debugVisualize(GfxContext* gfxc, RenderTarget* dstRenderTarget);
    };
#endif
}
