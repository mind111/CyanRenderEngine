#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "SparseVoxelOctreeDemo.h"
#include "Engine.h"
#include "GfxModule.h"
#include "VoxelBasedGI.h"
#include "Scene.h"
#include "GfxMaterial.h"
#include "GfxHardwareAbstraction/OpenGL/GLBuffer.h"
#include "GfxHardwareAbstraction/OpenGL/GLTexture.h"
#include "Color.h"

namespace Cyan
{
    static GLuint dummyVAO = -1;

    void SparseVoxelOctreeDemo::initialize()
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "BuildSparseVoxelOctree",
            [this](SceneView& view) {
                glm::uvec2 renderResolution(1920, 1080);
                GHTexture2D::Desc desc = GHTexture2D::Desc::create(renderResolution.x, renderResolution.y, 1, PF_RGB16F);
                GHSampler2D sampler2D;
                sampler2D.setFilteringModeMin(SAMPLER2D_FM_POINT);
                sampler2D.setFilteringModeMag(SAMPLER2D_FM_POINT);
                sampler2D.setAddressingModeX(SAMPLER2D_AM_CLAMP);
                sampler2D.setAddressingModeY(SAMPLER2D_AM_CLAMP);
                m_sceneColor = GHTexture2D::create(desc, sampler2D);
                m_sceneColorResolved = GHTexture2D::create(desc, sampler2D);
                m_indirectLighting = GHTexture2D::create(desc, sampler2D);
                m_prevFrameIndirectLighting = GHTexture2D::create(desc, sampler2D);

                GHDepthTexture::Desc depthDesc = GHDepthTexture::Desc::create(renderResolution.x, renderResolution.y, 1);
                m_sceneDepth = GHDepthTexture::create(depthDesc);

                glCreateVertexArrays(1, &dummyVAO);

                AxisAlignedBoundingBox3D sceneAABB;
                Scene* scene = view.getScene();
                for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
                {
                    auto subMeshInstance = scene->m_staticSubMeshInstances[i];
                    auto subMesh = subMeshInstance.subMesh;
                    glm::vec3 subMeshMin = subMesh->m_min;
                    glm::vec3 subMeshMax = subMesh->m_max;
                    glm::vec3 subMeshMinWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMin, 1.f);
                    glm::vec3 subMeshMaxWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMax, 1.f);
                    sceneAABB.bound(subMeshMinWS);
                    sceneAABB.bound(subMeshMaxWS);
                }

                glm::vec3 center = (sceneAABB.pmin + sceneAABB.pmax) * .5f;
                glm::vec3 extent = (sceneAABB.pmax - sceneAABB.pmin) * .5f;

                f32 sizeX = extent.x * 2.f / m_voxelGridResolution.x;
                f32 sizeY = extent.y * 2.f / m_voxelGridResolution.y;
                f32 sizeZ = extent.z * 2.f / m_voxelGridResolution.z;
                m_voxelSize = glm::vec3(glm::max(sizeX, sizeY));
                m_voxelSize = glm::vec3(glm::max(m_voxelSize, sizeZ));
                m_voxelGridCenter = center;
                m_voxelGridExtent = m_voxelGridResolution * .5f * m_voxelSize;
                m_voxelGridAABB.bound(m_voxelGridCenter + m_voxelGridExtent);
                m_voxelGridAABB.bound(m_voxelGridCenter - m_voxelGridExtent);

                sparseSceneVoxelization(scene, view.getRender(), view.getState());
            }
        );
    }

    void SparseVoxelOctreeDemo::render()
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "SparseVoxelOctreeDemo::render",
            [this](SceneView& view) {
                GPU_DEBUG_SCOPE(renderMarker, "Render SparseVoxelOctreeDemo")

                // clear scene color and scene depth
                RenderingUtils::clearTexture2D(m_sceneColor.get(), glm::vec4(0.f, 0.f, 0.f, 1.f));
                RenderingUtils::clearDepthTexture(m_sceneDepth.get(), 1.f);

                if (bVisualizeSVO)
                {
                    visualizeSVO(view, bVisualizeNodes, bVisualizeVoxels);
                }
                else if (bRaymarchingSVO)
                {
                    raymarchingSVO(view);
                }
                else
                {
                    // render indirect lighting here
                    renderSVOBasedLighting(view);
                }
            }
        );

        processUI();
    }

    struct VoxelFragment
    {
        glm::vec4 worldSpacePosition;
        glm::vec4 albedo;
        glm::vec4 normal;
        glm::vec4 directLighting;
    };

    std::unique_ptr<GHRWBuffer> SparseVoxelOctreeDemo::buildVoxelFragmentList(Scene* scene, SceneRender* render, const SceneView::State& viewState, u32& outNumVoxelFragments)
    {
        GPU_DEBUG_SCOPE(marker, "Build Voxel Fragment List")

        std::unique_ptr<GHAtomicCounterBuffer> voxelFragmentCounter = GHAtomicCounterBuffer::create();
        std::unique_ptr<GHRWBuffer> outVoxelFragmentBuffer = nullptr;

        // todo: changing this to numbers like 2048, 4096 seems to break stuffs, need to track down why
        glm::vec3 superSampledGridResolution = glm::vec3(1024.f);
        f32 viewports[] = {
            0, 0, superSampledGridResolution.z, superSampledGridResolution.y, // +x
            0, 0, superSampledGridResolution.x, superSampledGridResolution.z, // +y
            0, 0, superSampledGridResolution.x, superSampledGridResolution.y, // +z
        };
        glViewportArrayv(0, 3, viewports);

        // first pass count number voxel fragments
        {
            GPU_DEBUG_SCOPE(countVoxelFragments, "Count Voxel Fragments")

            bool bFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VoxelizeSceneVS", APP_SHADER_PATH "voxelize_scene_v.glsl");
            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VoxelizeSceneGS", APP_SHADER_PATH "voxelize_scene_g.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "CountVoxelFragmentPS", APP_SHADER_PATH "SVO/count_voxel_fragment_p.glsl");
            auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

            // no need to bind framebuffer here since we are not drawing to one
            auto gfxc = GfxContext::get();
            gfxc->disableDepthTest();
            gfxc->disableBlending();
            glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
            glDisable(GL_CULL_FACE);
            {
                p->bind();
                p->setUniform("u_voxelGridAABBMin", glm::vec3(m_voxelGridAABB.pmin));
                p->setUniform("u_voxelGridAABBMax", glm::vec3(m_voxelGridAABB.pmax));
                p->bindAtomicCounter("u_voxelFragmentCounter", voxelFragmentCounter.get());

                for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
                {
                    const auto& subMeshInstance = scene->m_staticSubMeshInstances[i];
                    p->setUniform("u_localToWorldMatrix", subMeshInstance.localToWorldMatrix);
                    subMeshInstance.subMesh->draw();
                }
                p->unbind();
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
        }

        // second pass fill in the buffer
        {
            GPU_DEBUG_SCOPE(countVoxelFragments, "Write Voxel Fragments")

            u32 numVoxelFragments = 0;
            voxelFragmentCounter->read(&numVoxelFragments);
            assert(numVoxelFragments > 0);
            outNumVoxelFragments = numVoxelFragments;

            // allocate just right amount of memory for voxel fragment buffer
            u32 bufferSize = numVoxelFragments * sizeof(VoxelFragment);
            outVoxelFragmentBuffer = GHRWBuffer::create(bufferSize);

            // reset the counter for the second pass
            numVoxelFragments = 0;
            voxelFragmentCounter->write(numVoxelFragments);

            bool bFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VoxelizeSceneVS", APP_SHADER_PATH "voxelize_scene_v.glsl");
            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VoxelizeSceneGS", APP_SHADER_PATH "voxelize_scene_g.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "BuildVoxelFragmentListPS", APP_SHADER_PATH "SVO/build_voxel_fragment_list_p.glsl");
            auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

            // no need to bind framebuffer here since we are not drawing to one
            auto gfxc = GfxContext::get();
            gfxc->disableDepthTest();
            gfxc->disableBlending();
            glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
            glDisable(GL_CULL_FACE);
            {
                p->bind();
                viewState.setShaderParameters(p.get());
                p->bindAtomicCounter("u_voxelFragmentCounter", voxelFragmentCounter.get());
                p->setRWBuffer("VoxelFragmentBuffer", outVoxelFragmentBuffer.get());
                p->setUniform("u_voxelGridCenter", m_voxelGridCenter);
                p->setUniform("u_voxelGridExtent", m_voxelGridExtent);
                p->setUniform("u_voxelGridResolution", m_voxelGridResolution);
                p->setUniform("u_voxelGridAABBMin", glm::vec3(m_voxelGridAABB.pmin));
                p->setUniform("u_voxelGridAABBMax", glm::vec3(m_voxelGridAABB.pmax));

                // bind data necessary for sun shadow
                if (scene->m_directionalLight != nullptr)
                {
                    p->setUniform("directionalLight.color", scene->m_directionalLight->m_color);
                    p->setUniform("directionalLight.intensity", scene->m_directionalLight->m_intensity);
                    p->setUniform("directionalLight.direction", scene->m_directionalLight->m_direction);
                    auto csm = render->m_csm.get();
                    csm->setShaderParameters(p.get());
                }

                for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
                {
                    const auto& subMeshInstance = scene->m_staticSubMeshInstances[i];
                    p->setUniform("u_localToWorldMatrix", subMeshInstance.localToWorldMatrix);

                    // todo: support texturing later
                    // get & set material parameters
                    glm::vec3 albedo(1.f);
                    subMeshInstance.material->getVec3("mp_albedo", albedo);
                    f32 roughness = .5f;
                    subMeshInstance.material->getFloat("mp_roughness", roughness);
                    f32 metallic = 0.f;
                    subMeshInstance.material->getFloat("mp_metallic", metallic);

                    p->setUniform("u_albedo", albedo);
                    p->setUniform("u_roughness", roughness);
                    p->setUniform("u_metallic", metallic);

                    subMeshInstance.subMesh->draw();
                }
                p->unbind();
            }
            glEnable(GL_CULL_FACE);
            glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
        }

        return std::move(outVoxelFragmentBuffer);
    }

    void SparseVoxelOctreeDemo::sparseSceneVoxelization(Scene* scene, SceneRender* render, const SceneView::State& viewState)
    {
        GPU_DEBUG_SCOPE(sparseVoxelization, "Sparse Scene Voxelization");

        // build voxel fragment list
        u32 numVoxelFragments = 0;
        std::unique_ptr<GHRWBuffer> voxelFragmentBuffer = buildVoxelFragmentList(scene, render, viewState, numVoxelFragments);
        assert(numVoxelFragments > 0);

        {
            GPU_DEBUG_SCOPE(buildSVO, "Build Sparse Voxel Octree");

            struct SVONode
            {
                glm::ivec4 coord;
                u32 bSubdivide;
                u32 bLeaf;
                u32 childIndex;
                u32 padding;
            };

            // estimating that the number of nodes is O(n^2) where n is the resolution of the grid
            u32 nodeBufferCapacity = m_voxelGridResolution.x * m_voxelGridResolution.y;
            u32 bufferSize = sizeof(SVONode) * nodeBufferCapacity;
            m_SVONodeBuffer = GHRWBuffer::create(bufferSize);

            std::unique_ptr<GHAtomicCounterBuffer> newNodesCounter = GHAtomicCounterBuffer::create();
            m_allocatedNodesCounter = GHAtomicCounterBuffer::create();
            m_allocatedVoxelsCounter = GHAtomicCounterBuffer::create();

            // initialize root node
            {
                SVONode rootNode;
                rootNode.coord = glm::ivec4(0, 0, 0, 0);
                rootNode.bSubdivide = 0;
                rootNode.bLeaf = 0;
                rootNode.childIndex = 0;
                rootNode.padding = 0;
                m_SVONodeBuffer->write(&rootNode, 0, 0, sizeof(rootNode));

                // set the allocated nodes count to 1 to start with
                u32 numAllocatedNodes = 1;
                m_allocatedNodesCounter->write(numAllocatedNodes);
            }

            m_SVONumLevels = glm::log2((u32)m_voxelGridResolution.x) + 1;
            for (i32 level = 0; level < m_SVONumLevels; ++level)
            {
                // mark nodes that need to be subdivided
                {
                    GPU_DEBUG_SCOPE(markSubdivision, "Mark Nodes For Subdivision");

                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOMarkSubdivisionCS", APP_SHADER_PATH "SVO/svo_mark_subdivision_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const static i32 localWorkGroupSizeX = 64;
                    i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [this, level, SVONodeBuffer = m_SVONodeBuffer.get(), voxelFragmentBuffer = voxelFragmentBuffer.get()](ComputePipeline* p) {
                            p->setRWBuffer("VoxelFragmentBuffer", voxelFragmentBuffer);
                            p->setRWBuffer("SVONodeBuffer", SVONodeBuffer);
                            p->setUniform("u_SVOCenter", m_voxelGridCenter);
                            p->setUniform("u_SVOExtent", m_voxelGridExtent);
                            p->setUniform("u_SVOAABBMin", m_voxelGridAABB.pmin);
                            p->setUniform("u_SVOAABBMax", m_voxelGridAABB.pmax);
                            p->setUniform("u_currentLevel", level);
                        },
                        workGroupSizeX,
                        1,
                        1
                        );
                }

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

                // count number nodes that needs to be subdivided and resize node buffer 
                // skip resizing the buffer for the very last pass since the last pass only marks
                // non-empty leaf nodes
                if (level < (m_SVONumLevels - 1))
                {
                    GPU_DEBUG_SCOPE(allocateNewNodes, "Count Subdivided Nodes");

                    // reset new nodes counter
                    u32 numNodesToSubdivide = 0;
                    newNodesCounter->write(numNodesToSubdivide);

                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOCountNewNodesCS", APP_SHADER_PATH "SVO/svo_count_new_nodes_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const static i32 localWorkGroupSizeX = 4;
                    const static i32 localWorkGroupSizeY = 4;
                    const static i32 localWorkGroupSizeZ = 4;

                    i32 levelSize = glm::pow(2, level); // 1, 2, 4, 8
                    i32 workGroupSizeX = glm::ceil((f32)levelSize / localWorkGroupSizeX);
                    i32 workGroupSizeY = glm::ceil((f32)levelSize / localWorkGroupSizeY);
                    i32 workGroupSizeZ = glm::ceil((f32)levelSize / localWorkGroupSizeZ);

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [this, level, newNodesCounter = newNodesCounter.get()](ComputePipeline* p) {
                            p->bindAtomicCounter("u_newNodesCounter", newNodesCounter);
                            p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                            p->setUniform("u_currentLevel", level);
                            p->setUniform("u_SVONumLevels", (i32)m_SVONumLevels);
                        },
                        workGroupSizeX,
                        workGroupSizeY,
                        workGroupSizeZ
                    );

                    glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);

                    newNodesCounter->read(&numNodesToSubdivide);
                    u32 newNodesToAllocate = numNodesToSubdivide * 8;
                    u32 numAllocatedNodes = 0;
                    m_allocatedNodesCounter->read(&numAllocatedNodes);
                    if (nodeBufferCapacity < (numAllocatedNodes + newNodesToAllocate))
                    {
                        u32 newNodeBufferCapacity = numAllocatedNodes + newNodesToAllocate;
                        u32 oldBufferSize = sizeof(SVONode) * nodeBufferCapacity;
                        u32 newBufferSize = sizeof(SVONode) * newNodeBufferCapacity;
                        std::unique_ptr<GHRWBuffer> newNodeBuffer = GHRWBuffer::create(newBufferSize);
                        GLShaderStorageBuffer* srcBuffer = dynamic_cast<GLShaderStorageBuffer*>(m_SVONodeBuffer.get());
                        GLShaderStorageBuffer* dstBuffer = dynamic_cast<GLShaderStorageBuffer*>(newNodeBuffer.get());
                        glCopyNamedBufferSubData(srcBuffer->getName(), dstBuffer->getName(), 0, 0, oldBufferSize);
                        nodeBufferCapacity = newNodeBufferCapacity;
                        m_SVONodeBuffer.swap(newNodeBuffer);
                    }
                }

                // allocate new nodes and initialize them
                {
                    GPU_DEBUG_SCOPE(allocateNewNodes, "Allocate New Nodes");

                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOAllocateNodesCS", APP_SHADER_PATH "SVO/svo_allocate_nodes_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const static i32 localWorkGroupSizeX = 4;
                    const static i32 localWorkGroupSizeY = 4;
                    const static i32 localWorkGroupSizeZ = 4;

                    i32 levelSize = glm::pow(2, level); // 1, 2, 4, 8
                    i32 workGroupSizeX = glm::ceil((f32)levelSize / localWorkGroupSizeX);
                    i32 workGroupSizeY = glm::ceil((f32)levelSize / localWorkGroupSizeY);
                    i32 workGroupSizeZ = glm::ceil((f32)levelSize / localWorkGroupSizeZ);

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [this, level, nodeBufferCapacity](ComputePipeline* p) {
                            p->bindAtomicCounter("u_allocatedNodesCounter", m_allocatedNodesCounter.get());
                            p->bindAtomicCounter("u_allocatedVoxelsCounter", m_allocatedVoxelsCounter.get());
                            p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                            p->setUniform("u_SVONodePoolSize", (i32)nodeBufferCapacity);
                            p->setUniform("u_currentLevel", level);
                            p->setUniform("u_SVONumLevels", (i32)m_SVONumLevels);
                        },
                        workGroupSizeX,
                        workGroupSizeY,
                        workGroupSizeZ
                    );
                }

                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
            }

            m_allocatedNodesCounter->read(&m_numNodes);
            m_allocatedVoxelsCounter->read(&m_numVoxels);

            const glm::uvec2 voxelPageResolution(1024, 1024);
            u32 numVoxelPages = glm::ceil((f32)m_numVoxels / (voxelPageResolution.x * voxelPageResolution.y));
            // write voxel data
            {
                GPU_DEBUG_SCOPE(writeVoxelData, "Write SVO Voxel Data");

                const auto desc = GHTexture3D::Desc::create(m_voxelGridResolution.x, m_voxelGridResolution.y, numVoxelPages, 1, PF_R32F);
                const auto& sampler3D = GHSampler3D::create();
                // these are temp resources so they are released after this rendering pass 
                std::unique_ptr<GHTexture3D> albedoRSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> albedoGSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> albedoBSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> normalXSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> normalYSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> normalZSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> directLightingRSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> directLightingGSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> directLightingBSum = GHTexture3D::create(desc, sampler3D);
                std::unique_ptr<GHTexture3D> perVoxelFragmentCounter = GHTexture3D::create(desc, sampler3D);

                // accumulating voxel attributes
                {
                    {
                        bool bFound = false;
                        auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOAccumulateVoxelDataCS", APP_SHADER_PATH "SVO/svo_accumulate_voxel_data_c.glsl");
                        auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                        const static i32 localWorkGroupSizeX = 64;
                        i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                        RenderingUtils::dispatchComputePass(
                            p.get(),
                            [this, albedoRSum = albedoRSum.get(), albedoGSum = albedoGSum.get(), albedoBSum = albedoBSum.get(), 
                            normalXSum = normalXSum.get(), normalYSum = normalYSum.get(), normalZSum = normalZSum.get(), perVoxelFragmentCounter = perVoxelFragmentCounter.get(), desc, voxelFragmentBuffer = voxelFragmentBuffer.get()](ComputePipeline* p) {
                                p->setUniform("u_SVOCenter", m_voxelGridCenter);
                                p->setUniform("u_SVOExtent", m_voxelGridExtent);
                                p->setUniform("u_SVOAABBMin", m_voxelGridAABB.pmin);
                                p->setUniform("u_SVOAABBMax", m_voxelGridAABB.pmax);
                                p->setUniform("u_SVOMaxLevelCount", (i32)m_SVONumLevels);
                                p->setUniform("u_voxelPoolSize", glm::ivec3(desc.width, desc.height, desc.depth));
                                p->setRWBuffer("VoxelFragmentBuffer", voxelFragmentBuffer);
                                p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                                p->setRWTexture("u_voxelAlbedoR", albedoRSum, 0);
                                p->setRWTexture("u_voxelAlbedoG", albedoGSum, 0);
                                p->setRWTexture("u_voxelAlbedoB", albedoBSum, 0);
                                p->setRWTexture("u_voxelNormalX", normalXSum, 0);
                                p->setRWTexture("u_voxelNormalY", normalYSum, 0);
                                p->setRWTexture("u_voxelNormalZ", normalZSum, 0);
                                p->setRWTexture("u_perVoxelFragmentCounter", perVoxelFragmentCounter, 0);
                            },
                            workGroupSizeX,
                            1,
                            1
                            );
                    }

                    {
                        bool bFound = false;
                        auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOAccumulateDirectLightingCS", APP_SHADER_PATH "SVO/svo_accumulate_direct_lighting_c.glsl");
                        auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                        const static i32 localWorkGroupSizeX = 64;
                        i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                        RenderingUtils::dispatchComputePass(
                            p.get(),
                            [this, directLightingR = directLightingRSum.get(), directLightingG = directLightingGSum.get(), directLightingB = directLightingBSum.get(), perVoxelFragmentCounter = perVoxelFragmentCounter.get(), desc, voxelFragmentBuffer = voxelFragmentBuffer.get()](ComputePipeline* p) {
                                p->setUniform("u_SVOCenter", m_voxelGridCenter);
                                p->setUniform("u_SVOExtent", m_voxelGridExtent);
                                p->setUniform("u_SVOAABBMin", m_voxelGridAABB.pmin);
                                p->setUniform("u_SVOAABBMax", m_voxelGridAABB.pmax);
                                p->setUniform("u_SVOMaxLevelCount", (i32)m_SVONumLevels);
                                p->setUniform("u_voxelPoolSize", glm::ivec3(desc.width, desc.height, desc.depth));
                                p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                                p->setRWBuffer("VoxelFragmentBuffer", voxelFragmentBuffer);
                                p->setRWTexture("u_voxelDirectLightingR", directLightingR, 0);
                                p->setRWTexture("u_voxelDirectLightingG", directLightingG, 0);
                                p->setRWTexture("u_voxelDirectLightingB", directLightingB, 0);
                            },
                            workGroupSizeX,
                            1,
                            1
                            );
                    }
                }
                // resolve average voxel attributes
                {
                    const auto& avgAlbedoDesc = GHTexture3D::Desc::create(m_voxelGridResolution.x, m_voxelGridResolution.y, numVoxelPages, 1, PF_RGBA32F);
                    m_voxelAlbedoPool = GHTexture3D::create(avgAlbedoDesc, sampler3D);
                    m_voxelNormalPool = GHTexture3D::create(avgAlbedoDesc, sampler3D);
                    m_voxelDirectLightingPool = GHTexture3D::create(avgAlbedoDesc, sampler3D);

                    // todo: it would be nice if I can put multiple shader content in one file...!
                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOResolveVoxelDataCS", APP_SHADER_PATH "SVO/svo_resolve_voxel_data_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const static i32 localWorkGroupSizeX = 64;
                    i32 workGroupSizeX = glm::ceil((f32)m_numVoxels / localWorkGroupSizeX);

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [this, albedoR = albedoRSum.get(), albedoG = albedoGSum.get(), albedoB = albedoBSum.get(), normalX = normalXSum.get(), normalY = normalYSum.get(), normalZ = normalZSum.get(), directLightingR = directLightingRSum.get(), directLightingG = directLightingGSum.get(), directLightingB = directLightingBSum.get(), perVoxelFragmentCounter = perVoxelFragmentCounter.get(), avgAlbedoDesc](ComputePipeline* p) {
                            p->setTexture("u_voxelAlbedoR", albedoR);
                            p->setTexture("u_voxelAlbedoG", albedoG);
                            p->setTexture("u_voxelAlbedoB", albedoB);
                            p->setTexture("u_voxelNormalX", normalX);
                            p->setTexture("u_voxelNormalY", normalY);
                            p->setTexture("u_voxelNormalZ", normalZ);
                            p->setTexture("u_voxelDirectLightingR", directLightingR);
                            p->setTexture("u_voxelDirectLightingG", directLightingG);
                            p->setTexture("u_voxelDirectLightingB", directLightingB);
                            p->setTexture("u_perVoxelFragmentCounter", perVoxelFragmentCounter);
                            p->setUniform("u_voxelPoolSize", glm::ivec3(avgAlbedoDesc.width, avgAlbedoDesc.height, avgAlbedoDesc.depth));
                            p->setRWTexture("u_voxelAlbedoPool", m_voxelAlbedoPool.get(), 0);
                            p->setRWTexture("u_voxelNormalPool", m_voxelNormalPool.get(), 0);
                            p->setRWTexture("u_voxelDirectLightingPool", m_voxelDirectLightingPool.get(), 0);
                        },
                        workGroupSizeX,
                        1,
                        1
                        );
                }
            }
        }
    }

    void SparseVoxelOctreeDemo::raymarchingSVO(SceneView& view)
    {
        GPU_DEBUG_SCOPE(raymarchingSVO, "Raymarching SVO");

        bool outFound = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "RayMarchingSVO", APP_SHADER_PATH "SVO/raymarching_svo_p.glsl");
        auto p = ShaderManager::findOrCreateGfxPipeline(outFound, vs, ps);

        auto render = view.getRender();
        const auto& desc = m_sceneColor->getDesc();
        const auto& viewState = view.getState();
        const auto& cameraInfo = view.getCameraInfo();

        RenderingUtils::renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [this](RenderPass& pass) {
                RenderTarget rt(m_sceneColor.get(), 0, false);
                pass.setRenderTarget(rt, 0);
            },
            p.get(),
                [this, desc, cameraInfo, viewState](GfxPipeline* p) {
                viewState.setShaderParameters(p);
                p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                p->setUniform("u_SVOCenter", m_voxelGridCenter);
                p->setUniform("u_SVOExtent", m_voxelGridExtent);
                p->setUniform("u_SVOAABBMin", glm::vec3(m_voxelGridAABB.pmin));
                p->setUniform("u_SVOAABBMax", glm::vec3(m_voxelGridAABB.pmax));
                p->setUniform("u_SVOMaxLevel", (i32)m_SVONumLevels);
                p->setUniform("u_renderResolution", glm::vec2(desc.width, desc.height));
                p->setUniform("u_cameraNearClippingPlane", cameraInfo.m_perspective.n);
                p->setUniform("u_cameraFov", cameraInfo.m_perspective.fov);
                p->setTexture("u_voxelAlbedoPool", m_voxelAlbedoPool.get());
                p->setTexture("u_voxelNormalPool", m_voxelNormalPool.get());
                p->setTexture("u_voxelDirectLightingPool", m_voxelDirectLightingPool.get());
            }
        );
    }

    void SparseVoxelOctreeDemo::visualizeSVO(SceneView& view, bool bVisualizeInternalNodes, bool bVisualizeLeafNodes)
    {
        GPU_DEBUG_SCOPE(raymarchingSVO, "Visualize SVO Voxels");

        auto render = view.getRender();
        const auto& desc = m_sceneColor->getDesc();
        const auto& viewState = view.getState();

        // copy over the scene depth
        RenderingUtils::copyTexture(m_sceneColor.get(), 0, render->color(), 0);
        RenderingUtils::copyDepthTexture(m_sceneDepth.get(), render->depth());

        // first pass drawing all the internal nodes
        if (bVisualizeInternalNodes)
        {
            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "VisualizeSVOVS", APP_SHADER_PATH "SVO/visualize_svo_v.glsl");
            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(outFound, "VisualizeSVOGS", APP_SHADER_PATH "SVO/visualize_svo_g.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "VisualizeSVOPS", APP_SHADER_PATH "SVO/visualize_svo_p.glsl");
            auto p = ShaderManager::findOrCreateGeometryPipeline(outFound, vs, gs, ps);

            RenderPass pass(desc.width, desc.height);
            RenderTarget rt(m_sceneColor.get(), 0, false);
            DepthTarget dt(m_sceneDepth.get(), false);
            pass.setRenderTarget(rt, 0);
            pass.setDepthTarget(dt);
            pass.setRenderFunc([p, this, viewState](GfxContext* ctx) {
                p->bind();
                viewState.setShaderParameters(p.get());
                p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                p->setUniform("u_SVOCenter", m_voxelGridCenter);
                p->setUniform("u_SVOExtent", m_voxelGridExtent);
                glBindVertexArray(dummyVAO);
                glDrawArrays(GL_POINTS, 0, m_numNodes);
                glBindVertexArray(0);
                p->unbind();
                });
            pass.enableDepthTest();
            pass.render(GfxContext::get());
        }

        // second pass drawing all the leaf node as filled cubes
        if (bVisualizeLeafNodes)
        {
            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "VisualizeSVOLeafVS", APP_SHADER_PATH "SVO/visualize_svo_leaf_v.glsl");
            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(outFound, "VisualizeSVOLeafGS", APP_SHADER_PATH "SVO/visualize_svo_leaf_g.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "VisualizeSVOLeafPS", APP_SHADER_PATH "SVO/visualize_svo_leaf_p.glsl");
            auto p = ShaderManager::findOrCreateGeometryPipeline(outFound, vs, gs, ps);

            RenderPass pass(desc.width, desc.height);
            RenderTarget rt(m_sceneColor.get(), 0, false);
            DepthTarget dt(m_sceneDepth.get(), false);
            pass.setRenderTarget(rt, 0);
            pass.setDepthTarget(dt);
            pass.setRenderFunc([this, p, viewState](GfxContext* ctx) {
                p->bind();
                viewState.setShaderParameters(p.get());
                p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                p->setUniform("u_SVOCenter", m_voxelGridCenter);
                p->setUniform("u_SVOExtent", m_voxelGridExtent);
                p->setTexture("u_voxelAlbedoPool", m_voxelAlbedoPool.get());
                glBindVertexArray(dummyVAO);
                glDrawArrays(GL_POINTS, 0, m_numNodes);
                glBindVertexArray(0);
                p->unbind();
                });
            pass.enableDepthTest();
            pass.disableBackfaceCulling();
            pass.render(GfxContext::get());
        }
    }

    void SparseVoxelOctreeDemo::renderSVOBasedLighting(SceneView& view)
    {
        const auto& viewState = view.getState();
        auto scene = view.getScene();
        auto render = view.getRender();

        {
            GPU_DEBUG_SCOPE(renderSVODirectLighting, "Render SVO DirectLighting");

            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "RenderSVODirectLighting", APP_SHADER_PATH "SVO/svo_render_direct_lighting_p.glsl");
            auto p = ShaderManager::findOrCreateGfxPipeline(outFound, vs, ps);

            const auto& desc = m_sceneColor->getDesc();

            RenderingUtils::renderSkybox(m_sceneColor.get(), m_sceneDepth.get(), scene->m_skybox, viewState.viewMatrix, viewState.projectionMatrix, 0);

            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this](RenderPass& pass) {
                    RenderTarget rt(m_sceneColor.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                },
                p.get(),
                [viewState, scene, render, this](GfxPipeline* p) {
                    viewState.setShaderParameters(p);
                    p->setTexture("u_sceneAlbedo", render->albedo());
                    p->setTexture("u_sceneDepth", render->depth());
                    p->setTexture("u_sceneNormal", render->normal());
                    p->setTexture("u_sceneMetallicRoughness", render->metallicRoughness());
                    p->setUniform("u_SVOCenter", m_voxelGridCenter);
                    p->setUniform("u_SVOExtent", m_voxelGridExtent);
                    p->setUniform("u_SVOAABBMin", glm::vec3(m_voxelGridAABB.pmin));
                    p->setUniform("u_SVOAABBMax", glm::vec3(m_voxelGridAABB.pmax));
                    p->setUniform("u_SVONumLevels", (i32)(m_SVONumLevels));
                    p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                    p->setUniform("u_voxelSize", m_voxelSize);
                    if (scene->m_directionalLight != nullptr)
                    {
                        p->setUniform("u_sunDirection", scene->m_directionalLight->m_direction);
                        glm::vec3 sunColor = scene->m_directionalLight->m_color * scene->m_directionalLight->m_intensity;
                        p->setUniform("u_sunColor", sunColor);
                    }
                }
            );
        }

        {
            GPU_DEBUG_SCOPE(renderSVOIndirectLighting, "Render SVO Indirect Lighting");

            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "RenderSVOIndirectLighting", APP_SHADER_PATH "SVO/render_indirect_lighting_p.glsl");
            auto p = ShaderManager::findOrCreateGfxPipeline(outFound, vs, ps);

            const auto& desc = m_indirectLighting->getDesc();

            // reset rendering when the view changes
            bool bViewChanged = (viewState.viewMatrix != viewState.prevFrameViewMatrix);
            if (bViewChanged)
            {
                m_accumulatedSampleCount = 0;
                RenderingUtils::clearTexture2D(m_prevFrameIndirectLighting.get(), glm::vec4(0.f, 0.f, 0.f, 1.f));
            }

            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this](RenderPass& pass) {
                    RenderTarget rt(m_indirectLighting.get());
                    pass.setRenderTarget(rt, 0);
                },
                p.get(),
                [viewState, scene, render, this](GfxPipeline* p) {
                    viewState.setShaderParameters(p);
                    p->setTexture("u_sceneAlbedo", render->albedo());
                    p->setTexture("u_sceneDepth", render->depth());
                    p->setTexture("u_sceneNormal", render->normal());
                    p->setTexture("u_sceneMetallicRoughness", render->metallicRoughness());
                    p->setUniform("u_SVOCenter", m_voxelGridCenter);
                    p->setUniform("u_SVOExtent", m_voxelGridExtent);
                    p->setUniform("u_SVOAABBMin", glm::vec3(m_voxelGridAABB.pmin));
                    p->setUniform("u_SVOAABBMax", glm::vec3(m_voxelGridAABB.pmax));
                    p->setUniform("u_SVONumLevels", (i32)(m_SVONumLevels));
                    p->setRWBuffer("SVONodeBuffer", m_SVONodeBuffer.get());
                    p->setTexture("u_voxelAlbedoPool", m_voxelAlbedoPool.get());
                    p->setTexture("u_voxelNormalPool", m_voxelNormalPool.get());
                    p->setTexture("u_voxelDirectLightingPool", m_voxelDirectLightingPool.get());
                    p->setUniform("u_sampleCount", (i32)m_accumulatedSampleCount);
                    p->setTexture("u_prevFrameSceneColor", m_prevFrameIndirectLighting.get());
                    p->setUniform("u_voxelSize", m_voxelSize);

                    if (scene->m_skyLight)
                    {
                        p->setTexture("u_skyLight", scene->m_skyLight->getReflectionCubemap()->cubemap.get());
                    }
                }
            );

            if (viewState.frameCount > 0)
            {
                RenderingUtils::copyTexture(m_prevFrameIndirectLighting.get(), 0, m_indirectLighting.get(), 0);
            }

            m_accumulatedSampleCount = glm::min<i32>(1024, m_accumulatedSampleCount + 1);
        }

        {
            GPU_DEBUG_SCOPE(combineSVOLighting, "Combine Lighting");

            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "CombineSVOLighting", APP_SHADER_PATH "SVO/combine_lighting_p.glsl");
            auto p = ShaderManager::findOrCreateGfxPipeline(outFound, vs, ps);

            const auto& desc = m_sceneColor->getDesc();

            auto ctx = GfxContext::get();
            ctx->enableBlending();
            ctx->setBlendingMode(BlendingMode::kAdditive);
            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this](RenderPass& pass) {
                    RenderTarget rt(m_sceneColor.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                },
                p.get(),
                [viewState, scene, render, this](GfxPipeline* p) {
                    viewState.setShaderParameters(p);
                    p->setTexture("u_indirectLighting", m_indirectLighting.get());
                }
            );
            GfxContext::get()->disableBlending();
        }

        RenderingUtils::tonemapping(m_sceneColorResolved.get(), m_sceneColor.get());

        // draw a progress bar for visualizing convergence
        if (m_accumulatedSampleCount < 1024)
        {
            bool outFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "ConstantColorPS", ENGINE_SHADER_PATH "constant_color_p.glsl");
            auto p = ShaderManager::findOrCreateGfxPipeline(outFound, vs, ps);

            glm::uvec2 offset(64);
            u32 backgroundBarWidth = 1920 - offset.x * 2;
            const u32 backgroundBarHeight = 32;

            const auto& desc = m_sceneColorResolved->getDesc();
            // draw progress bar backgroud
            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this, offset, backgroundBarWidth, backgroundBarHeight](RenderPass& pass) {
                    RenderTarget rt(m_sceneColorResolved.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                    pass.setViewport(offset.x, offset.y, backgroundBarWidth, backgroundBarHeight);
                },
                p.get(),
                [viewState, scene, render, this](GfxPipeline* p) {
                    p->setUniform("u_color", glm::vec4(Color::white, 1.f));
                }
            );

            // draw progress bar
            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [this, offset, backgroundBarWidth, backgroundBarHeight](RenderPass& pass) {
                    RenderTarget rt(m_sceneColorResolved.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                    u32 barWidth = backgroundBarWidth * 0.998;
                    u32 barHeight = backgroundBarHeight * 0.95;
                    u32 offsetX = (backgroundBarWidth - barWidth) / 2;
                    u32 offsetY = (backgroundBarHeight - barHeight) / 2;

                    f32 progress = m_accumulatedSampleCount / (f32)1024;
                    pass.setViewport(offset.x + offsetX, offset.y + offsetY, barWidth * progress, barHeight);
                },
                p.get(),
                [viewState, scene, render, this](GfxPipeline* p) {
                    p->setUniform("u_color", glm::vec4(Color::green, 1.f));
                }
            );
        }
    }

    void SparseVoxelOctreeDemo::processUI()
    {
        // UI
        Engine::get()->enqueueUICommand([this](ImGuiContext* imguiCtx) { 
                const float indentation = 10.f;
                ImGui::SetCurrentContext(imguiCtx);
                ImGui::Begin("Sparse Voxel Octree");
                {
                    enum class VisualizationModes
                    {
                        kVisualizeSVO = 0,
                        kRaymarchingSVO,
                        kNone,
                        kCount
                    };
                    const char* visModeNames[(i32)VisualizationModes::kCount] = {
                        "Visualize SVO",
                        "Raymarching SVO",
                        "None"
                    };

                    static VisualizationModes s_svoVisMode = VisualizationModes::kNone;

                    ImGui::Combo("Visualizations", (i32*)(&s_svoVisMode), visModeNames, (i32)VisualizationModes::kCount);

                    switch (s_svoVisMode)
                    {
                    case VisualizationModes::kVisualizeSVO:
                        bVisualizeSVO = true;
                        ImGui::Indent(indentation);
                        IMGUI_CHECKBOX("Visualize Nodes", bVisualizeNodes);
                        IMGUI_CHECKBOX("Visualize Voxels", bVisualizeVoxels);
                        ImGui::Unindent();
                        break;
                    case VisualizationModes::kRaymarchingSVO:
                        bVisualizeSVO = false;
                        bRaymarchingSVO = true;
                        break;
                    case VisualizationModes::kNone:
                        bVisualizeSVO = false;
                        bRaymarchingSVO = false;
                        break;
                    default:
                        assert(0);
                    }
                }
                ImGui::End();
            });
    }
}