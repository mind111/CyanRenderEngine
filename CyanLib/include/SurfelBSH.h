#pragma once

#include <functional>

#include "Common.h"
#include "glm/glm.hpp"
#include "ShaderStorageBuffer.h"

namespace Cyan {
    class GfxContext;
    struct RenderTarget;

    struct Surfel {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 albedo;
        f32 radius;
    };

    struct GpuSurfel {
        glm::vec4 positionAndRadius;
        glm::vec4 normal;
        glm::vec4 albedo;
        glm::vec4 radiance;
    };

    struct InstanceDesc {
        glm::mat4 transform;
        glm::vec4 albedo;
        glm::vec4 radiance;
        glm::vec4 debugColor;
    };

    /** 
    * Surfel Bounding Sphere Hierarchy
    */
    struct SurfelBSH {
        struct Node {
            void build(const std::vector<Surfel>& surfels, u32 surfelIndex);
            void build(const std::vector<Node>& nodes, u32 leftChildIndex, u32 rightChildIndex);

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

        friend class MicroRenderingGI;

        virtual void build(const std::vector<Surfel>& inSurfels);

        Node* root = nullptr;
        std::vector<Surfel> surfels;
        std::vector<Node> nodes;
        ShaderStorageBuffer<DynamicSsboData<GpuNode>> gpuNodes;
    protected:
        virtual u32 getNumLevels() { return numLevels; }
        ShaderStorageBuffer<DynamicSsboData<InstanceDesc>> nodeInstanceBuffer;
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
        ShaderStorageBuffer<DynamicSsboData<InstanceDesc>> nodeInstanceBuffer;
        i32 activeVisLevel = 0;
        i32 numVisLevels = 1;
        void buildInternal(i32 parent, std::vector<Surfel>& surfels, u32 start, u32 end);
        bool bVisualizeBoundingSpheres = true, bVisualizeNodes = true;
        void visualize(GfxContext* gfxc, RenderTarget* dstRenderTarget);
        void debugVisualize(GfxContext* gfxc, RenderTarget* dstRenderTarget);
    };
#endif
}
