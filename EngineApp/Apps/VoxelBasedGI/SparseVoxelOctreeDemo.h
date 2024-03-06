#pragma once

#include "glew/glew.h"

#include "Scene.h"
#include "SceneView.h"
#include "SceneRender.h"
#include "RenderingUtils.h"

namespace Cyan
{
    class GHAtomicCounterBuffer;

    class SparseVoxelOctreeDemo
    {
    public:
        SparseVoxelOctreeDemo() { }
        ~SparseVoxelOctreeDemo() { }

        void initialize();
        void render();

        glm::vec3 m_voxelGridResolution = glm::vec3(1024);
        glm::vec3 m_voxelGridCenter;
        glm::vec3 m_voxelGridExtent;
        glm::vec3 m_voxelSize;
        AxisAlignedBoundingBox3D m_voxelGridAABB;

        u32 m_SVONumLevels = 0;
        std::unique_ptr<GHAtomicCounterBuffer> m_allocatedNodesCounter = nullptr;
        u32 m_numNodes;
        std::unique_ptr<GHAtomicCounterBuffer> m_allocatedVoxelsCounter = nullptr;
        u32 m_numVoxels;
        std::unique_ptr<GHRWBuffer> m_SVONodeBuffer;

        // voxel pools
        std::unique_ptr<GHTexture3D> m_voxelAlbedoPool = nullptr;
        std::unique_ptr<GHTexture3D> m_voxelNormalPool = nullptr;
        std::unique_ptr<GHTexture3D> m_voxelDirectLightingPool = nullptr;
        std::unique_ptr<GHTexture2D> m_sceneColor = nullptr;
        std::unique_ptr<GHTexture2D> m_sceneColorResolved = nullptr;
        std::unique_ptr<GHTexture2D> m_prevFrameIndirectLighting = nullptr;
        std::unique_ptr<GHTexture2D> m_indirectLighting = nullptr;
        std::unique_ptr<GHDepthTexture> m_sceneDepth = nullptr;
    private:
        std::unique_ptr<GHRWBuffer> buildVoxelFragmentList(Scene* scene, SceneRender* render, const SceneView::State& viewState, u32& outNumVoxelFragments);
        void sparseSceneVoxelization(Scene* scene, SceneRender* render, const SceneView::State& viewState);
        void raymarchingSVO(SceneView& view);
        void visualizeSVO(SceneView& view, bool bVisualizeInternalNodes, bool bVisualizeLeafNodes);
        void renderSVOBasedLighting(SceneView& view);
        void processUI();

        bool bVisualizeSVO = false;
        bool bVisualizeNodes = false;
        bool bVisualizeVoxels = false;
        bool bRaymarchingSVO = false;
        u32 m_accumulatedSampleCount = 0;
    };
}
