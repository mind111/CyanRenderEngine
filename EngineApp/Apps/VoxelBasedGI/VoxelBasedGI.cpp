#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "VoxelBasedGI.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"
#include "Scene.h"
#include "SceneView.h"
#include "SceneRender.h"
#include "RenderingUtils.h"
#include "Color.h"
#include "GfxModule.h"
#include "glew/glew.h"
#include "GfxHardwareAbstraction/OpenGL/GLTexture.h"
#include "GfxMaterial.h"
#include "AssetManager.h"
#include "StaticMesh.h"
#include "MathLibrary.h"
#include "GfxHardwareAbstraction/GHInterface/GHBuffer.h"

namespace Cyan
{

    VoxelBasedGI::VoxelBasedGI(u32 windowWidth, u32 windowHeight)
        : App("Voxel Based GI Test", windowWidth, windowHeight)
    {
        m_sparseVoxelOctreeDemo = std::make_unique<SparseVoxelOctreeDemo>();
    }

    VoxelBasedGI::~VoxelBasedGI()
    {

    }

    void VoxelBasedGI::update(World* world)
    {

    }

    static void renderVoxelBasedAO()
    {
    }

    // todo: implement caching material attributes and direct radiance or even indirect irradiance
    // todo: figure out how to deal with the ray start offsetting issues (acne caused by self intersection or incorrect intersection)
    // todo: do faithful voxel ray marching first and then move on to cone tracing

    // todo: using world space position to sample the voxel irradiance cache
    // todo: add a debug visualization for voxels and ray start position

    // todo: D: exploring irradiance cache using volume texture and

    static GLuint dummyVAO = -1;
    static const glm::vec3 s_voxelGridResolution = glm::vec3(1024);
    static i32 s_numMips = 1;
    static glm::vec3 s_voxelGridCenter;
    static glm::vec3 s_voxelGridExtent;
    static glm::vec3 s_voxelSize;
    static AxisAlignedBoundingBox3D s_voxelGridAABB;
    static AxisAlignedBoundingBox3D s_sceneAABB;
    static const i32 maxNumRayMarchingIterations = 64;

    static std::unique_ptr<GHTexture3D> s_voxelGridOpacity = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridFilteredOpacity = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridFragmentCounter = nullptr;

    // scene normal
    static std::unique_ptr<GHTexture3D> s_voxelGridNormalX = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridNormalY = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridNormalZ = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridNormal = nullptr;

    // scene direct radiance cache
    static std::unique_ptr<GHTexture3D> s_voxelGridDirectRadianceR = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridDirectRadianceG = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridDirectRadianceB = nullptr;
    static std::unique_ptr<GHTexture3D> s_voxelGridDirectRadiance = nullptr;

    static std::unique_ptr<GHTexture2D> s_voxelGridVisA = nullptr;
    static std::unique_ptr<GHTexture2D> s_voxelGridVisB = nullptr;
    static std::unique_ptr<GHTexture2D> s_voxelGridVisC = nullptr;
    static std::unique_ptr<GHTexture2D> s_voxelGridVisD = nullptr;

    // rendering output
    static std::unique_ptr<GHTexture2D> s_sceneColor;
    static std::unique_ptr<GHTexture2D> s_prevFrameSceneColor = nullptr;
    static std::unique_ptr<GHTexture2D> s_resolvedSceneColor = nullptr;

    static void voxelizeScene(SceneView& view)
    {
        Scene* scene = view.getScene();
        for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
        {
            auto subMeshInstance = scene->m_staticSubMeshInstances[i];
            auto subMesh = subMeshInstance.subMesh;
            glm::vec3 subMeshMin = subMesh->m_min;
            glm::vec3 subMeshMax = subMesh->m_max;
            glm::vec3 subMeshMinWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMin, 1.f);
            glm::vec3 subMeshMaxWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMax, 1.f);
            s_sceneAABB.bound(subMeshMinWS);
            s_sceneAABB.bound(subMeshMaxWS);
        }

        auto render = view.getRender();
        const auto& viewState = view.getState();

        glm::vec3 center = (s_sceneAABB.pmin + s_sceneAABB.pmax) * .5f;
        glm::vec3 extent = (s_sceneAABB.pmax - s_sceneAABB.pmin) * .5f;

        f32 sizeX = extent.x * 2.f / s_voxelGridResolution.x;
        f32 sizeY = extent.y * 2.f / s_voxelGridResolution.y;
        f32 sizeZ = extent.z * 2.f / s_voxelGridResolution.z;
        s_voxelSize = glm::vec3(glm::max(sizeX, sizeY));
        s_voxelSize = glm::vec3(glm::max(s_voxelSize, sizeZ));

        s_voxelGridCenter = center;
        s_voxelGridExtent = s_voxelGridResolution * .5f * s_voxelSize;
        s_voxelGridAABB.bound(s_voxelGridCenter + s_voxelGridExtent);
        s_voxelGridAABB.bound(s_voxelGridCenter - s_voxelGridExtent);

        {
            GPU_DEBUG_SCOPE(voxelizeScene, "Scene Voxelization")

            bool bFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VoxelizeSceneVS", APP_SHADER_PATH "voxelize_scene_v.glsl");
            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VoxelizeSceneGS", APP_SHADER_PATH "voxelize_scene_g.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VoxelizeScenePS", APP_SHADER_PATH "voxelize_scene_p.glsl");
            auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

            // super sampling, regardless of what the voxel grid resolution is, always rasterize
            // at a relatively high resolution to provide better sampling frequency. This help with
            // missing some part of the geometry during voxelization, and provide better filtering for
            // each voxel cell
            glm::vec3 superSampledGridResolution = glm::vec3(1024.f);

            f32 viewports[] = {
                0, 0, superSampledGridResolution.z, superSampledGridResolution.y, // +x
                0, 0, superSampledGridResolution.x, superSampledGridResolution.z, // +y
                0, 0, superSampledGridResolution.x, superSampledGridResolution.y, // +z
            };
            glViewportArrayv(0, 3, viewports);

            // no need to bind framebuffer here since we are not drawing to one
            auto gfxc = GfxContext::get();
            gfxc->disableDepthTest();
            gfxc->disableBlending();

            p->bind();
            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);

            p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
            p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));

            // bind data necessary for sun shadow
            if (scene->m_directionalLight != nullptr)
            {
                p->setUniform("directionalLight.color", scene->m_directionalLight->m_color);
                p->setUniform("directionalLight.intensity", scene->m_directionalLight->m_intensity);
                p->setUniform("directionalLight.direction", scene->m_directionalLight->m_direction);
                auto csm = render->m_csm.get();
                csm->setShaderParameters(p.get());
            }

            // write opacity
            auto opacityTex = dynamic_cast<GLTexture3D*>(s_voxelGridOpacity.get());
            glBindImageTexture(0, opacityTex->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

            // write normal
            auto normalXTex = dynamic_cast<GLTexture3D*>(s_voxelGridNormalX.get());
            glBindImageTexture(1, normalXTex->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            auto normalYTex = dynamic_cast<GLTexture3D*>(s_voxelGridNormalY.get());
            glBindImageTexture(2, normalYTex->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            auto normalZTex = dynamic_cast<GLTexture3D*>(s_voxelGridNormalZ.get());
            glBindImageTexture(3, normalZTex->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

            // write direct radiance
            auto radianceR = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceR.get());
            glBindImageTexture(4, radianceR->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            auto radianceG = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceG.get());
            glBindImageTexture(5, radianceG->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
            auto radianceB = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceB.get());
            glBindImageTexture(6, radianceB->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

            // write fragment counter
            auto counterTex = dynamic_cast<GLTexture3D*>(s_voxelGridFragmentCounter.get());
            glBindImageTexture(7, counterTex->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

            glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
            glDisable(GL_CULL_FACE);
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
            glEnable(GL_CULL_FACE);
            glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
            p->unbind();
        }

        {
            GPU_DEBUG_SCOPE(calcVoxelGridNormal, "Build Voxel Grid Normal And Radiance")

            bool bFound = false;
            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "BuildVoxelGridAttributesCS", APP_SHADER_PATH "build_voxel_grid_attributes_c.glsl");
            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

            const auto& desc = s_voxelGridNormal->getDesc();
            i32 imageWidth = desc.width;
            i32 imageHeight = desc.height;
            i32 imageDepth = desc.depth;

            i32 workGroupX = glm::max(imageWidth / 8, 1); 
            i32 workGroupY = glm::max(imageHeight / 8, 1); 
            i32 workGroupZ = glm::max(imageDepth / 8, 1);

            RenderingUtils::dispatchComputePass(
                p.get(),
                [imageWidth, imageHeight, imageDepth](ComputePipeline* p) {
                    auto normalX = dynamic_cast<GLTexture3D*>(s_voxelGridNormalX.get());
                    auto normalY = dynamic_cast<GLTexture3D*>(s_voxelGridNormalY.get());
                    auto normalZ = dynamic_cast<GLTexture3D*>(s_voxelGridNormalZ.get());
                    auto normal = dynamic_cast<GLTexture3D*>(s_voxelGridNormal.get());
                    glBindImageTexture(0, normalX->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(1, normalY->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(2, normalZ->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(3, normal->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

                    auto radianceR = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceR.get());
                    auto radianceG = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceG.get());
                    auto radianceB = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadianceB.get());
                    auto radiance = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadiance.get());
                    glBindImageTexture(4, radianceR->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(5, radianceG->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(6, radianceB->getName(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
                    glBindImageTexture(7, radiance->getName(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

                    // bind this 3d texture as a texture instead of image due to running out of available image unit bindings
                    p->setTexture("u_voxelGridFragmentCounter", s_voxelGridFragmentCounter.get());
                    p->setUniform("u_imageSize", glm::ivec3(imageWidth, imageHeight, imageDepth));
                },
                workGroupX,
                workGroupY,
                workGroupZ
            );
        }

        {
            GPU_DEBUG_SCOPE(buildHierarchicalVoxelGrd, "Build Hierarchical Voxel Grid")

            bool bFound = false;
            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "BuildHierarchicalVoxelGridCS", APP_SHADER_PATH "build_hierarchical_voxel_grid_c.glsl");
            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

            assert(isPowerOf2(s_voxelGridResolution.x) && isPowerOf2(s_voxelGridResolution.y) && isPowerOf2(s_voxelGridResolution.z));
            i32 size = glm::min(glm::min(s_voxelGridResolution.x, s_voxelGridResolution.y), s_voxelGridResolution.z);
            u32 numMips = glm::log2(size) + 1;
            u32 numPasses = numMips - 1;
            for (u32 pass = 0; pass < numPasses; ++pass)
            {
                u32 srcMip = pass;
                u32 dstMip = pass + 1;
                i32 dstWidth, dstHeight, dstDepth;
                s_voxelGridOpacity->getMipSize(dstWidth, dstHeight, dstDepth, dstMip);
                i32 workGroupX = glm::max(dstWidth / 8, 1); 
                i32 workGroupY = glm::max(dstHeight / 8, 1); 
                i32 workGroupZ = glm::max(dstDepth / 8, 1);
                RenderingUtils::dispatchComputePass(
                    p.get(),
                    [srcMip, dstMip, dstWidth, dstHeight, dstDepth](ComputePipeline* p) {
                        auto glTex3D = dynamic_cast<GLTexture3D*>(s_voxelGridOpacity.get());
                        if (glTex3D != nullptr)
                        {
                            glBindImageTexture(0, glTex3D->getName(), srcMip, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
                            glBindImageTexture(1, glTex3D->getName(), dstMip, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
                        }
                        p->setUniform("u_imageSize", glm::ivec3(dstWidth, dstHeight, dstDepth));
                    },
                    workGroupX,
                    workGroupY,
                    workGroupZ
                );
            }
        }
    }

    static void visualizeVoxelGrid(GHTexture2D* dstTexture, SceneView& view, const glm::vec3& voxelSize, GHTexture3D* opacityTex, GHTexture3D* targetTex, glm::vec3 voxelGridResolution, const AxisAlignedBoundingBox3D& voxelGridAABB, const glm::vec3& voxelGridCenter, const glm::vec3& voxelGridExtent, i32 mipLevel)
    {
        const i32 maxNumIterations = 128;

        bool bFound = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "RayMarchingVoxelGridPS", APP_SHADER_PATH "visualize_voxel_grid_p.glsl");
        auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

        const auto& desc = dstTexture->getDesc();
        const auto& cameraInfo = view.getCameraInfo();
        const auto& viewState = view.getState();

        RenderingUtils::renderScreenPass(
            glm::uvec2(desc.width, desc.height),
            [dstTexture](RenderPass& pass) {
                RenderTarget rt(dstTexture, 0);
                pass.setRenderTarget(rt, 0);
            },
            p.get(),
            [desc, cameraInfo, viewState, opacityTex, targetTex, voxelSize, voxelGridAABB, voxelGridCenter, voxelGridExtent, voxelGridResolution, mipLevel](GfxPipeline* gfxp) {
                viewState.setShaderParameters(gfxp);

                gfxp->setUniform("u_voxelGridAABBMin", glm::vec3(voxelGridAABB.pmin));
                gfxp->setUniform("u_voxelGridAABBMax", glm::vec3(voxelGridAABB.pmax));
                gfxp->setUniform("u_voxelSize", s_voxelSize);
                gfxp->setUniform("u_voxelGridCenter", voxelGridCenter);
                gfxp->setUniform("u_voxelGridExtent", voxelGridExtent);
                gfxp->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                gfxp->setUniform("u_renderResolution", glm::vec2(desc.width, desc.height));
                gfxp->setUniform("u_cameraNearClippingPlane", cameraInfo.m_perspective.n);
                gfxp->setUniform("u_cameraFov", cameraInfo.m_perspective.fov);
                gfxp->setTexture("u_voxelGridOpacityTex", opacityTex);
                gfxp->setTexture("u_voxelGridTargetTex", targetTex);

                gfxp->setUniform("u_numMipLevels", (i32)opacityTex->getDesc().numMips);
                gfxp->setUniform("u_maxNumIterations", (i32)maxNumIterations);
                gfxp->setUniform("u_mipLevel", mipLevel);
            }
        );
    }

    static void visualizeVoxels(GHTexture2D* dstTexture, GHDepthTexture* depthTexture, const SceneView::State& viewState)
    {
        GPU_DEBUG_SCOPE(marker, "Visualize Voxels")

        bool bFound = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeVoxelsVS", APP_SHADER_PATH "visualize_voxels_v.glsl");
        auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VisualizeVoxelsGS", APP_SHADER_PATH "visualize_voxels_g.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeVoxelsPS", APP_SHADER_PATH "visualize_voxels_p.glsl");
        auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);
        const auto& desc = dstTexture->getDesc();
        RenderPass pass(desc.width, desc.height);
        RenderTarget rt(dstTexture, 0, false);
        pass.setRenderTarget(rt, 0);
        DepthTarget dt(depthTexture, false);
        pass.setDepthTarget(dt);
        pass.setRenderFunc([viewState, p](GfxContext* ctx) {
            p->bind();
            viewState.setShaderParameters(p.get());
            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
            p->setUniform("u_voxelSize", s_voxelSize);
            p->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());

            glBindVertexArray(dummyVAO);
            i32 numVertices = s_voxelGridResolution.x * s_voxelGridResolution.y * s_voxelGridResolution.z;
            glDrawArrays(GL_POINTS, 0, numVertices);
            p->unbind();
        });
        pass.enableDepthTest();
        pass.render(GfxContext::get());
    }

    static bool bVisualizeVoxels = false;

    // diffuse screen space traces 
    static std::unique_ptr<GHTexture2D> s_ssDiffuseRayInfo = nullptr;
    static std::unique_ptr<GHTexture2D> s_ssDiffuseHitMask = nullptr;

    // reflection screen space traces 
    static std::unique_ptr<GHTexture2D> s_ssReflectionRayInfo = nullptr;
    static std::unique_ptr<GHTexture2D> s_ssReflectionHitMask = nullptr;
    static void initExperiementA(u32 windowWidth, u32 windowHeight)
    {
        {
            const auto& desc = GHTexture2D::Desc::create(windowWidth, windowHeight, 1, PF_RGBA16F);
            GHSampler2D sampler;
            s_ssDiffuseRayInfo = GHTexture2D::create(desc, sampler);
        }
        {
            const auto& desc = GHTexture2D::Desc::create(windowWidth, windowHeight, 1, PF_R8);
            GHSampler2D sampler;
            s_ssDiffuseHitMask = GHTexture2D::create(desc, sampler);
        }
    }

    // todo A: exploring casting 1 ray per pixel and connecting screen space traces with voxel space traces
    void VoxelBasedGI::experiementA()
    {
        // [] add a debug visualization for voxel based ray tracing
        // [] add a debug visualization for voxels and ray start position

        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "Voxel Based Rendering Experiement A",
            [](SceneView& view) {
                auto scene = view.getScene();
                auto render = view.getRender();
                const auto& viewState = view.getState();

                voxelizeScene(view);

                {
                    GPU_DEBUG_SCOPE(marker, "Visualize Voxel Grid")
                    {
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Opacity")
                        }
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Normal")
                            visualizeVoxelGrid(s_voxelGridVisB.get(), view, s_voxelSize, s_voxelGridOpacity.get(), s_voxelGridNormal.get(), s_voxelGridResolution, s_voxelGridAABB, s_voxelGridCenter, s_voxelGridExtent, 0);
                        }
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Direct Radiance")
                            visualizeVoxelGrid(s_voxelGridVisC.get(), view, s_voxelSize, s_voxelGridOpacity.get(), s_voxelGridDirectRadiance.get(), s_voxelGridResolution, s_voxelGridAABB, s_voxelGridCenter, s_voxelGridExtent, 0);
                        }
                    }
                }

#if 0
                {
                    GPU_DEBUG_SCOPE(marker, "Voxel Based Ray Tracing")

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VoxelBasedRTXPS", APP_SHADER_PATH "voxel_based_raytracing_p.glsl");
                    auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);
                    const auto& desc = s_voxelBasedRayTracing->getDesc();

                    auto scene = view.getScene();
                    RenderingUtils::renderScreenPass(
                        glm::uvec2(desc.width, desc.height),
                        [](RenderPass& pass) {
                            RenderTarget rt(s_voxelBasedRayTracing.get(), 0);
                            rt.clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
                            pass.setRenderTarget(rt, 0);
                        },
                        p.get(),
                        [desc, scene, render, viewState](GfxPipeline* gfxp) {
                            viewState.setShaderParameters(gfxp);

                            gfxp->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                            gfxp->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                            gfxp->setUniform("u_voxelSize", s_voxelSize);
                            gfxp->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            gfxp->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                            gfxp->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                            gfxp->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());
                            gfxp->setTexture("u_voxelGridNormalTex", s_voxelGridNormal.get());
                            gfxp->setTexture("u_voxelGridRadianceTex", s_voxelGridDirectRadiance.get());

                            gfxp->setUniform("u_numMipLevels", (i32)s_voxelGridOpacity->getDesc().numMips);
                            gfxp->setUniform("u_maxNumIterations", (i32)maxNumRayMarchingIterations);

                            gfxp->setTexture("u_sceneDepthTex", render->depth());
                            gfxp->setTexture("u_sceneNormalTex", render->normal());
                            gfxp->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                            gfxp->setTexture("u_sceneAlbedoTex", render->albedo());
                            gfxp->setTexture("u_prevFrameSceneDepthTex", render->depth());
                            gfxp->setTexture("u_prevFrameVBRTTex", s_prevFrameVoxelBasedRayTracing.get());
                            gfxp->setTexture("u_sceneNormalTex", render->normal());

                            if (scene->m_skyLight != nullptr)
                            {
                                gfxp->setTexture("u_skyCubemapTex", scene->m_skyLight->getReflectionCubemap()->cubemap.get());
                            }

                            if (scene->m_directionalLight != nullptr)
                            {
                                glm::vec3 sunDirection = scene->m_directionalLight->m_direction;
                                gfxp->setUniform("u_sunDirection", sunDirection);
                                glm::vec3 sunColor = scene->m_directionalLight->m_color * scene->m_directionalLight->m_intensity;
                                gfxp->setUniform("u_sunColor", sunColor);
                            }
                        });

                    RenderingUtils::copyTexture(s_prevFrameVoxelBasedRayTracing.get(), 0, s_voxelBasedRayTracing.get(), 0);
                    RenderingUtils::renderSkybox(s_voxelBasedRayTracing.get(), render->depth(), scene->m_skybox, viewState.viewMatrix, viewState.projectionMatrix, 0);
                    RenderingUtils::postprocessing(s_resolvedVBRTColor.get(), s_voxelBasedRayTracing.get());
                }
#else // ray continuation experiements
                {
                    GPU_DEBUG_SCOPE(marker, "Hybrid Voxel Based Ray Tracing")

                    // do screen space traces first
                    {
                        GPU_DEBUG_SCOPE(marker, "Screen Space Trace")

                        bool bFound = false;
                        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "ScreenTracesPS", APP_SHADER_PATH "screen_space_traces_p.glsl");
                        auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);
                        const auto& desc = s_ssDiffuseRayInfo->getDesc();

                        RenderingUtils::renderScreenPass(
                            glm::uvec2(desc.width, desc.height),
                            [](RenderPass& pass) {
                                RenderTarget rt0(s_ssDiffuseHitMask.get(), 0);
                                pass.setRenderTarget(rt0, 0);

                                RenderTarget rt1(s_ssDiffuseRayInfo.get(), 0);
                                pass.setRenderTarget(rt1, 1);
                            },
                            p.get(),
                            [desc, render, viewState](GfxPipeline* gfxp) {
                                viewState.setShaderParameters(gfxp);
                                auto hiz = render->hiz();
                                gfxp->setTexture("hiz.depthQuadtree", hiz->m_depthBuffer.get());
                                gfxp->setUniform("hiz.numMipLevels", (i32)(hiz->m_depthBuffer->getDesc().numMips));
                                gfxp->setUniform("settings.kMaxNumIterationsPerRay", 64);

                                gfxp->setTexture("u_sceneDepthTex", render->depth());
                                gfxp->setTexture("u_sceneNormalTex", render->normal());
                            });
                    }

                    {
                        GPU_DEBUG_SCOPE(marker, "Voxel Space Trace")

                        bool bFound = false;
                        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "HybridVoxelBasedRTXPS", APP_SHADER_PATH "hybrid_voxel_based_raytracing_p.glsl");
                        auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);
                        const auto& desc = s_sceneColor->getDesc();

                        auto scene = view.getScene();
                        RenderingUtils::renderScreenPass(
                            glm::uvec2(desc.width, desc.height),
                            [](RenderPass& pass) {
                                RenderTarget rt(s_sceneColor.get(), 0);
                                rt.clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
                                pass.setRenderTarget(rt, 0);
                            },
                            p.get(),
                            [desc, scene, render, viewState](GfxPipeline* gfxp) {
                                viewState.setShaderParameters(gfxp);

                                gfxp->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                                gfxp->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                                gfxp->setUniform("u_voxelSize", s_voxelSize);
                                gfxp->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                                gfxp->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                                gfxp->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                                gfxp->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());
                                gfxp->setTexture("u_voxelGridNormalTex", s_voxelGridNormal.get());
                                gfxp->setTexture("u_voxelGridRadianceTex", s_voxelGridDirectRadiance.get());

                                gfxp->setUniform("u_numMipLevels", (i32)s_voxelGridOpacity->getDesc().numMips);
                                gfxp->setUniform("u_maxNumIterations", (i32)maxNumRayMarchingIterations);

                                gfxp->setTexture("u_sceneDepthTex", render->depth());
                                gfxp->setTexture("u_sceneNormalTex", render->normal());
                                gfxp->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                                gfxp->setTexture("u_sceneAlbedoTex", render->albedo());
                                gfxp->setTexture("u_prevFrameSceneDepthTex", render->depth());
                                gfxp->setTexture("u_prevFrameVBRTTex", s_prevFrameSceneColor.get());
                                gfxp->setTexture("u_sceneNormalTex", render->normal());

                                // ray continuation
                                gfxp->setTexture("u_ssDiffuseHitMask", s_ssDiffuseHitMask.get());
                                gfxp->setTexture("u_ssDiffuseRayInfo", s_ssDiffuseRayInfo.get());

                                if (scene->m_skyLight != nullptr)
                                {
                                    gfxp->setTexture("u_skyCubemapTex", scene->m_skyLight->getReflectionCubemap()->cubemap.get());
                                }

                                if (scene->m_directionalLight != nullptr)
                                {
                                    glm::vec3 sunDirection = scene->m_directionalLight->m_direction;
                                    gfxp->setUniform("u_sunDirection", sunDirection);
                                    glm::vec3 sunColor = scene->m_directionalLight->m_color * scene->m_directionalLight->m_intensity;
                                    gfxp->setUniform("u_sunColor", sunColor);
                                }
                            });
                    }

                    RenderingUtils::copyTexture(s_prevFrameSceneColor.get(), 0, s_sceneColor.get(), 0);
                    RenderingUtils::renderSkybox(s_sceneColor.get(), render->depth(), scene->m_skybox, viewState.viewMatrix, viewState.projectionMatrix, 0);
                    RenderingUtils::postprocessing(s_resolvedSceneColor.get(), s_sceneColor.get());
                }
#endif

                // todo: [] draw a debug visualization to inspect ray casting
                if (bVisualizeVoxels)
                {
                    GPU_DEBUG_SCOPE(marker, "Visualize Voxels")

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeVoxelsVS", APP_SHADER_PATH "visualize_voxels_v.glsl");
                    auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VisualizeVoxelsGS", APP_SHADER_PATH "visualize_voxels_g.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeVoxelsPS", APP_SHADER_PATH "visualize_voxels_p.glsl");
                    auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);
                    const auto& desc = s_resolvedSceneColor->getDesc();
                    RenderPass pass(desc.width, desc.height);
                    RenderTarget rt(s_resolvedSceneColor.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                    DepthTarget dt(render->depth(), false);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([viewState, p](GfxContext* ctx) {
                        p->bind();
                        viewState.setShaderParameters(p.get());
                        p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                        p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                        p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                        p->setUniform("u_voxelSize", s_voxelSize);
                        p->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());

                        glBindVertexArray(dummyVAO);
                        i32 numVertices = s_voxelGridResolution.x * s_voxelGridResolution.y * s_voxelGridResolution.z;
                        glDrawArrays(GL_POINTS, 0, numVertices);
                        p->unbind();
                    });
                    pass.enableDepthTest();
                    pass.render(GfxContext::get());
                }

                // draw some debug visualizations here
                {
                    // 1. visualize screen space hit mask
                    RenderingUtils::copyTexture(s_voxelGridVisA.get(), 0, s_ssDiffuseHitMask.get(), 0);

                    // 2. visualize screen space ray
                    {
                        GPU_DEBUG_SCOPE(marker, "Visualize Screen Space Trace")

                        const auto& desc = s_resolvedSceneColor->getDesc();
                        static std::unique_ptr<GHDepthTexture> tempDepthTexture = GHDepthTexture::create(GHDepthTexture::Desc::create(desc.width, desc.height, 1));

                        // since we are sampling and writing to depth at the same time, need to use a copy
                        // of the depth buffer for writing
                        RenderingUtils::copyDepthTexture(tempDepthTexture.get(), render->depth());
                        {
                            bool bFound = false;
                            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeScreenSpaceRayVS", APP_SHADER_PATH "visualize_screen_space_ray_v.glsl");
                            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VisualizeScreenSpaceRayGS", APP_SHADER_PATH "visualize_screen_space_ray_g.glsl");
                            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeScreenSpaceRayPS", APP_SHADER_PATH "visualize_screen_space_ray_p.glsl");
                            auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);
                            RenderPass pass(desc.width, desc.height);
                            RenderTarget rt(s_resolvedSceneColor.get(), 0, false);
                            pass.setRenderTarget(rt, 0);
                            DepthTarget dt(tempDepthTexture.get(), false);

                            pass.setDepthTarget(dt);
                            pass.setRenderFunc([viewState, p, render](GfxContext* ctx) {
                                p->bind();
                                viewState.setShaderParameters(p.get());
                                p->setTexture("u_sceneDepthTex", render->depth());
                                p->setTexture("u_sceneNormalTex", render->normal());

                                glBindVertexArray(dummyVAO);
                                glDrawArrays(GL_POINTS, 0, 1);
                                p->unbind();
                            });
                            pass.enableDepthTest();
                            pass.render(GfxContext::get());
                        }

                        {
                            bool bFound = false;
                            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeScreenSpaceRayVS", APP_SHADER_PATH "visualize_screen_space_ray_v.glsl");
                            auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VisualizeScreenSpaceRayStartEndGS", APP_SHADER_PATH "visualize_screen_space_ray_start_end_g.glsl");
                            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeScreenSpaceRayPS", APP_SHADER_PATH "visualize_screen_space_ray_p.glsl");
                            auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);
                            RenderPass pass(desc.width, desc.height);
                            RenderTarget rt(s_resolvedSceneColor.get(), 0, false);
                            pass.setRenderTarget(rt, 0);
                            DepthTarget dt(tempDepthTexture.get(), false);

                            pass.setDepthTarget(dt);
                            pass.setRenderFunc([viewState, p, render](GfxContext* ctx) {
                                p->bind();
                                viewState.setShaderParameters(p.get());
                                p->setTexture("u_sceneDepthTex", render->depth());
                                p->setTexture("u_sceneNormalTex", render->normal());

                                glBindVertexArray(dummyVAO);
                                glDrawArrays(GL_POINTS, 0, 1);
                                p->unbind();
                            });
                            pass.enableDepthTest();
                            pass.render(GfxContext::get());
                        }
                    }
                }

                // RenderingUtils::drawAABB(s_resolvedVBRTColor.get(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, s_sceneAABB, glm::vec4(Color::cyan, 1.f));
                // RenderingUtils::drawAABB(s_resolvedVBRTColor.get(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, s_voxelGridAABB, glm::vec4(Color::cyan, 1.f));
            });

            Engine::get()->enqueueFrameGfxTask(
                RenderingStage::kPostRenderToBackBuffer,
                "RenderDebugVisualizations", 
                [this](Frame& frame) {
                    glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                    glm::ivec2 resolution(480, 270);
                    RenderingUtils::renderToBackBuffer(s_resolvedSceneColor.get(), Viewport { 0, 0, (i32)windowSize.x, (i32)windowSize.y } );
                    RenderingUtils::renderToBackBuffer(s_voxelGridVisA.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 1, (i32)resolution.x, (i32)resolution.y } );
                    RenderingUtils::renderToBackBuffer(s_voxelGridVisB.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 2, (i32)resolution.x, (i32)resolution.y } );
                    RenderingUtils::renderToBackBuffer(s_voxelGridVisC.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 3, (i32)resolution.x, (i32)resolution.y } );
                    RenderingUtils::renderToBackBuffer(s_sceneColor.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 4, (i32)resolution.x, (i32)resolution.y } );
                });

            // UI
            Engine::get()->enqueueUICommand([](ImGuiContext* imguiCtx) { 
                    ImGui::SetCurrentContext(imguiCtx);
                    ImGui::Begin("Voxel Based Ray Tracing");
                    {
                        IMGUI_CHECKBOX("Visualize Voxel Cubes", bVisualizeVoxels)
                    }
                    ImGui::End();
                });
    }

    static i32 s_visualizationMipLevel = 0;
    static i32 s_maxMarchingSteps = 32;
    static bool bVisualizeSamplesAlongCone = false;
    static bool bVisualizeConeTracedPrimaryView = false;
    static f32 s_halfConeAperture = 15.f;
    static f32 s_coneNormalOffset = 0.15f;
    static f32 s_coneDirectionOffset = 0.f;
    static f32 s_maxTraceDistance = 100.f;

    // cone vis parameters
    static f32 s_coneVisTheta = 0.f;
    static f32 s_coneVisD = .5f;
    static f32 s_coneVisHalfAperture = 15.f;
    static f32 s_coneVisPhi[7] = { 0.f, 0.f, 60.f, 120.f, 180.f, 240.f, 300.f };
    static f32 s_debugConeHalfAngle = .1f;
    static bool bVisualizeDiffuseCones = false;
    static bool bFreezeConeVisualization = false;
    static f32 s_nearFieldConeTraceDistance = .1f;
    static f32 s_nearFieldConeTraceAperture = 15.f;

    // todo: render this low res buffer with a large cone angle
    static std::unique_ptr<GHTexture2D> s_lowResConeTracing = nullptr;
    static void initExperiementB(u32 windowWidth, u32 windowHeight)
    {

    }

    // todo B: exploring cone tracing
    void VoxelBasedGI::experiementB()
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "Voxel Based Rendering Experiement A",
            [](SceneView& view) {
                auto scene = view.getScene();
                auto render = view.getRender();
                const auto& viewState = view.getState();

                voxelizeScene(view);

                {
                    GPU_DEBUG_SCOPE(marker, "Filtering Voxel Grid")

                    // copy over the opacity grid
                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "CopyTexture3D", APP_SHADER_PATH "copy_texture_3d_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const auto& desc = s_voxelGridOpacity->getDesc();
                    i32 workGroupX = desc.width / 8;
                    i32 workGroupY = desc.height / 8;
                    i32 workGroupZ = desc.depth / 8;

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [scene, desc, viewState](ComputePipeline* p) {
                            auto opacityPyramid = dynamic_cast<GLTexture3D*>(s_voxelGridFilteredOpacity.get());
                            glBindImageTexture(0, opacityPyramid->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8);
                            p->setUniform("u_imageSize", glm::ivec3(desc.width, desc.height, desc.depth));
                            p->setTexture("u_voxelGridOpacity", s_voxelGridOpacity.get());
                        },
                        workGroupX,
                        workGroupY,
                        workGroupZ
                    );
                    auto voxelOpacity = dynamic_cast<GLTexture3D*>(s_voxelGridFilteredOpacity.get());
                    glGenerateTextureMipmap(voxelOpacity->getName());

                    // hardware mipmapping 
                    auto voxelRadianceTex = dynamic_cast<GLTexture3D*>(s_voxelGridDirectRadiance.get());
                    glGenerateTextureMipmap(voxelRadianceTex->getName());
                }

                {
                    GPU_DEBUG_SCOPE(marker, "Visualize Voxel Grid")
                    {
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Opacity")
                            visualizeVoxelGrid(s_voxelGridVisA.get(), view, s_voxelSize, s_voxelGridOpacity.get(), s_voxelGridFilteredOpacity.get(), s_voxelGridResolution, s_voxelGridAABB, s_voxelGridCenter, s_voxelGridExtent, s_visualizationMipLevel);
                        }
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Normal")
                            visualizeVoxelGrid(s_voxelGridVisB.get(), view, s_voxelSize, s_voxelGridOpacity.get(), s_voxelGridNormal.get(), s_voxelGridResolution, s_voxelGridAABB, s_voxelGridCenter, s_voxelGridExtent, s_visualizationMipLevel);
                        }
                        {
                            GPU_DEBUG_SCOPE(marker, "Visualize Direct Radiance")
                            visualizeVoxelGrid(s_voxelGridVisC.get(), view, s_voxelSize, s_voxelGridOpacity.get(), s_voxelGridDirectRadiance.get(), s_voxelGridResolution, s_voxelGridAABB, s_voxelGridCenter, s_voxelGridExtent, s_visualizationMipLevel);
                        }
                    }
                }

                // cone tracing diffuse pass
                {
                    GPU_DEBUG_SCOPE(marker, "Cone Traced Diffuse")

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "ConeTracedDiffuseGIPS", APP_SHADER_PATH "cone_traced_diffuse_gi_p.glsl");
                    auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

                    const auto& desc = s_sceneColor->getDesc();

                    RenderingUtils::renderScreenPass(
                        glm::uvec2(desc.width, desc.height),
                        [](RenderPass& pass) {
                            RenderTarget rt(s_sceneColor.get(), 0);
                            pass.setRenderTarget(rt, 0);
                        },
                        p.get(),
                        [desc, viewState, render, scene](GfxPipeline* p) {
                            viewState.setShaderParameters(p);

                            p->setTexture("u_sceneDepthTex", render->depth());
                            p->setTexture("u_sceneNormalTex", render->normal());
                            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                            p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                            p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                            p->setUniform("u_voxelSize", s_voxelSize);
                            p->setUniform("u_renderResolution", glm::vec2(desc.width, desc.height));

                            p->setTexture("u_opacity", s_voxelGridFilteredOpacity.get());
                            p->setTexture("u_radiance", s_voxelGridDirectRadiance.get());
                            p->setUniform("u_maxMarchingSteps", s_maxMarchingSteps);
                            p->setUniform("u_halfConeAperture", s_halfConeAperture);
                            p->setUniform("u_coneNormalOffset", s_coneNormalOffset);
                            p->setUniform("u_coneDirectionOffset", s_coneDirectionOffset);
                            p->setUniform("u_maxTraceDistance", s_maxTraceDistance);
                            p->setTexture("u_skyCubemap", scene->m_skyLight->getReflectionCubemap()->cubemap.get());
                            p->setUniform("u_debugConeHalfAngle", s_debugConeHalfAngle);

                            p->setTexture("u_opacityHierarchy", s_voxelGridOpacity.get());
                            p->setUniform("u_numMipLevels", (i32)s_voxelGridOpacity->getDesc().numMips);
                            p->setUniform("u_maxNumIterations", (i32)maxNumRayMarchingIterations);
                            p->setUniform("u_nearFieldConeTraceDistance", s_nearFieldConeTraceDistance);
                            p->setUniform("u_nearFieldConeTraceAperture", s_nearFieldConeTraceAperture);
                        }
                    );
                }

                // cone tracing reflection pass
                {
                    GPU_DEBUG_SCOPE(marker, "Cone Traced Reflection")
                }

                // cone tracing primary view
                if (bVisualizeConeTracedPrimaryView)
                {
                    GPU_DEBUG_SCOPE(marker, "Visualize Cone Tracing")

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeConeTracingPS", APP_SHADER_PATH "cone_tracing_primary_view_p.glsl");
                    auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

                    const auto& desc = s_sceneColor->getDesc();
                    const auto& cameraInfo = view.getCameraInfo();
                    RenderingUtils::renderScreenPass(
                        glm::uvec2(desc.width, desc.height),
                        [](RenderPass& pass) {
                            RenderTarget rt(s_sceneColor.get(), 0);
                            pass.setRenderTarget(rt, 0);
                        },
                        p.get(),
                        [desc, viewState, cameraInfo](GfxPipeline* p) {
                            viewState.setShaderParameters(p);

                            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                            p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                            p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                            p->setUniform("u_voxelSize", s_voxelSize);
                            p->setUniform("u_renderResolution", glm::vec2(desc.width, desc.height));
                            p->setUniform("u_cameraNearClippingPlane", cameraInfo.m_perspective.n);
                            p->setUniform("u_cameraFov", cameraInfo.m_perspective.fov);

                            p->setTexture("u_opacity", s_voxelGridFilteredOpacity.get());
                            p->setTexture("u_radiance", s_voxelGridDirectRadiance.get());
                            p->setUniform("u_maxMarchingSteps", s_maxMarchingSteps);
                        }
                    );
                }

                // visualizing / debugging a cone trace 
                if (bVisualizeSamplesAlongCone)
                {
                    GPU_DEBUG_SCOPE(marker, "Debug Cone Tracing")

                    // fix the cone trace origin and direction
                    static glm::vec3 debugTraceOrigin = viewState.cameraPosition;
                    static glm::vec3 debugTraceDirection = viewState.cameraForward;

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeConeTraceVS", APP_SHADER_PATH "visualize_cone_trace_v.glsl");
                    auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VisualizeConeTraceGS", APP_SHADER_PATH "visualize_cone_trace_g.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeConeTracePS", APP_SHADER_PATH "visualize_cone_trace_p.glsl");
                    auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

                    const auto& desc = s_sceneColor->getDesc();
                    const auto& cameraInfo = view.getCameraInfo();
                    RenderPass pass(desc.width, desc.height);
                    RenderTarget rt(s_sceneColor.get(), 0, false);
                    pass.setRenderTarget(rt, 0);
                    DepthTarget dt(render->depth(), false);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([viewState, cameraInfo, desc, p](GfxContext* ctx) {
                            glDisable(GL_CULL_FACE);
                            p->bind();
                            viewState.setShaderParameters(p.get());

                            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                            p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                            p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                            p->setUniform("u_voxelSize", s_voxelSize);
                            p->setUniform("u_renderResolution", glm::vec2(desc.width, desc.height));
                            p->setUniform("u_cameraNearClippingPlane", cameraInfo.m_perspective.n);
                            p->setUniform("u_cameraFov", cameraInfo.m_perspective.fov);

                            p->setTexture("u_opacity", s_voxelGridFilteredOpacity.get());
                            p->setTexture("u_radiance", s_voxelGridDirectRadiance.get());

                            p->setUniform("u_ro", debugTraceOrigin);
                            p->setUniform("u_rd", debugTraceDirection);

                            glBindVertexArray(dummyVAO);
                            glDrawArrays(GL_POINTS, 0, 64);

                            p->unbind();
                            glEnable(GL_CULL_FACE);
                        });
                    pass.enableDepthTest();
                    pass.render(GfxContext::get());
                }

                if (bVisualizeDiffuseCones)
                {
                    GPU_DEBUG_SCOPE(marker, "Visualize Cones")

                    static std::unique_ptr<GHRWBuffer> coneTransformBuffer = GHRWBuffer::create(sizeof(glm::mat4) * 7);
                    static std::unique_ptr<GHRWBuffer> coneOffsetBuffer = GHRWBuffer::create(sizeof(glm::vec4) * 2);

                    if (!bFreezeConeVisualization)
                    {
                        GPU_DEBUG_SCOPE(marker, "Build Cone Data")

                        // launch a compute pass that read the pixel normal and write data to a ssbo
                        bool bFound = false;
                        auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "DrawConeCS", APP_SHADER_PATH "draw_cone_c.glsl");
                        auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);


                        RenderingUtils::dispatchComputePass(
                            p.get(),
                            [viewState, render](ComputePipeline* p) {
                                coneTransformBuffer->bind();
                                coneOffsetBuffer->bind();
                                viewState.setShaderParameters(p);
                                p->setTexture("u_sceneDepthTex", render->depth());
                                p->setTexture("u_sceneNormalTex", render->normal());
                                p->setUniform("u_coneNormalOffset", s_coneNormalOffset);
                                p->setUniform("u_coneDirectionOffset", s_coneDirectionOffset);
                            },
                            1,
                            1,
                            1
                            );
                    }

                    // make sure that write to ssbo is visible to the following geometry pass
                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                    {
                        GPU_DEBUG_SCOPE(marker, "Draw Normal Offset")

                        // read back data from gpu
                        glm::vec4 data[2] = { };
                        coneOffsetBuffer->read(data, 0, 0, sizeof(glm::vec4) * 2);
                        // draw a line from world space position to cone origin
                        RenderingUtils::drawWorldSpaceLine(s_sceneColor.get(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, data[0], data[1], glm::vec4(Color::cyan, 1.f));

                        // draw world space position without any cone offset
                        RenderingUtils::drawWorldSpacePoint(s_sceneColor.get(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, data[0], glm::vec4(Color::pink, 1.f), 10.f);
                        // draw cone origin
                        RenderingUtils::drawWorldSpacePoint(s_sceneColor.get(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, data[1], glm::vec4(Color::purple, 1.f), 10.f);
                    }

                    {
                        GPU_DEBUG_SCOPE(marker, "Draw Cone")

                        // launch a geometry pass that reads the data wrote to that ssbo by the compute and draw cones
                        bool bFound = false;
                        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "DrawConeVS", APP_SHADER_PATH "draw_cone_v.glsl");
                        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "DrawConePS", APP_SHADER_PATH "draw_cone_p.glsl");
                        auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

                        static StaticMesh* debugConeMesh = AssetManager::findAsset<StaticMesh>("InvertedCone");
                        static const f32 defaultAperture = glm::atan(0.5f);

                        const auto& desc = s_sceneColor->getDesc();
                        RenderPass pass(desc.width, desc.height);
                        RenderTarget rt(s_sceneColor.get(), 0, false);
                        pass.setRenderTarget(rt, 0);
                        DepthTarget dt(render->depth(), false);
                        pass.setDepthTarget(dt);
                        pass.setRenderFunc([p, viewState](GfxContext* ctx) {
                            p->bind();
                            coneTransformBuffer->bind();
                            viewState.setShaderParameters(p.get());

#if 0
                            for (i32 i = 0; i < 7; ++i)
                            {
                                p->setUniform("u_coneIndex", i);
                                if (debugConeMesh != nullptr)
                                {
                                    for (i32 i = 0; i < debugConeMesh->numSubMeshes(); ++i)
                                    {
                                        GfxStaticSubMesh* sm = (*debugConeMesh)[i]->getGfxMesh();
                                        sm->draw();
                                    }
                                }
                            }
#else
                            for (i32 i = 0; i < 6; ++i)
                            {
                                p->setUniform("u_coneIndex", i);
                                if (debugConeMesh != nullptr)
                                {
                                    for (i32 i = 0; i < debugConeMesh->numSubMeshes(); ++i)
                                    {
                                        GfxStaticSubMesh* sm = (*debugConeMesh)[i]->getGfxMesh();
                                        sm->draw();
                                    }
                                }
                            }
#endif

                            p->unbind();
                        });
                        pass.enableDepthTest();
                        pass.disableBackfaceCulling();

                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
                        pass.render(GfxContext::get());
                        glDisable(GL_BLEND);
                    }
                }

                if (bVisualizeVoxels)
                {
                    visualizeVoxels(s_sceneColor.get(), render->depth(), viewState);
                }
            }
        );

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPostRenderToBackBuffer,
            "RenderDebugVisualizations", 
            [this](Frame& frame) {
                glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                glm::ivec2 resolution(480, 270);
                // RenderingUtils::renderToBackBuffer(s_voxelGridVisC.get(), Viewport { 0, 0, (i32)windowSize.x, (i32)windowSize.y } );
                RenderingUtils::renderToBackBuffer(s_sceneColor.get(), Viewport { 0, 0, (i32)windowSize.x, (i32)windowSize.y } );
                RenderingUtils::renderToBackBuffer(s_voxelGridVisA.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 1, (i32)resolution.x, (i32)resolution.y } );
                RenderingUtils::renderToBackBuffer(s_voxelGridVisB.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 2, (i32)resolution.x, (i32)resolution.y } );
                RenderingUtils::renderToBackBuffer(s_voxelGridVisC.get(), Viewport { 0, (i32)windowSize.y - resolution.y * 3, (i32)resolution.x, (i32)resolution.y } );
            });

        // UI
        Engine::get()->enqueueUICommand([](ImGuiContext* imguiCtx) { 
                ImGui::SetCurrentContext(imguiCtx);
                ImGui::Begin("Voxel Cone Tracing");
                {
                    IMGUI_SLIDER_INT("Visualization Mip Level", &s_visualizationMipLevel, 0, s_numMips);
                    IMGUI_SLIDER_INT("Marching Steps", &s_maxMarchingSteps, 32, 128);
                    IMGUI_CHECKBOX("Visualize Samples Along Cone", bVisualizeSamplesAlongCone);
                    IMGUI_CHECKBOX("Visualize Cone Traced Primary View", bVisualizeConeTracedPrimaryView);
                    IMGUI_SLIDER_FLOAT("Diffsue Cone Half Aperture", &s_halfConeAperture, 1.f, 36.f);
                    IMGUI_CHECKBOX("Visualize Voxels", bVisualizeVoxels);
                    IMGUI_SLIDER_FLOAT("Cone Normal Offset", &s_coneNormalOffset, 0.f, 1.f);
                    IMGUI_SLIDER_FLOAT("Max Cone Trace Distance", &s_maxTraceDistance, 0.f, 100.f);
                    IMGUI_SLIDER_FLOAT("Debug Half Cone Angle", &s_debugConeHalfAngle, 0.f, 30.f);
                    IMGUI_CHECKBOX("Visualize Diffuse Cones", bVisualizeDiffuseCones);
                    if (bVisualizeDiffuseCones)
                    {
                        IMGUI_CHECKBOX("Freeze Cone Visualization", bFreezeConeVisualization);
                    }
                    IMGUI_SLIDER_FLOAT("Near Field Cone Trace Distance", &s_nearFieldConeTraceDistance, 0.f, 1.f);
                    IMGUI_SLIDER_FLOAT("Near Field Cone Trace Aperture", &s_nearFieldConeTraceAperture, 1.f, 30.f);

#if 0
                    IMGUI_SLIDER_FLOAT("Cone Vis D", &s_coneVisD, 0.f, 2.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Half Aperture", &s_coneVisHalfAperture, 0.f, 45.f);

                    IMGUI_SLIDER_FLOAT("Cone Vis Theta", &s_coneVisTheta, 0.f, 90.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 0", &s_coneVisPhi[0], 0.f, 360.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 1", &s_coneVisPhi[1], 0.f, 360.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 2", &s_coneVisPhi[2], 0.f, 360.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 3", &s_coneVisPhi[3], 0.f, 360.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 4", &s_coneVisPhi[4], 0.f, 360.f);
                    IMGUI_SLIDER_FLOAT("Cone Vis Phi 5", &s_coneVisPhi[5], 0.f, 360.f);
#endif
                }
                ImGui::End();
            });
    }

    static void initExperiementC(u32 windowWidth, u32 windowHeight)
    {

    }

    // todo C: exploring a hybrid approach using voxel cone tracing and voxel ray marching 
    void VoxelBasedGI::experiementC()
    {
    }

    static std::unique_ptr<GHTexture3D> s_voxelGridIrradiance = nullptr;
    static std::unique_ptr<GHTexture2D> s_voxelGridIrradianceVisualization = nullptr;
    static void initExperiementD(u32 windowWidth, u32 windowHeight)
    {
        GHSampler3D trilinearSampler;
        trilinearSampler.setFilteringModeMin(SAMPLER3D_FM_TRILINEAR);
        trilinearSampler.setFilteringModeMag(SAMPLER3D_FM_TRILINEAR);

        const auto& irradianceDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, 1, PF_RGBA16F);
        s_voxelGridIrradiance = GHTexture3D::create(irradianceDesc, trilinearSampler);

        const auto& desc = GHTexture2D::Desc::create(windowWidth, windowHeight, 1, PF_RGB16F);
        s_voxelGridIrradianceVisualization = GHTexture2D::create(desc, GHSampler2D());
    }

    // todo D: exploring irradiance caching using something like octree texture
    void VoxelBasedGI::experiementD()
    {
        // [] sampling the irradiance cache 3d texture using world space position with trilinear filtering and see how it looks
        //     [] trilinear sampling is working, but because of the trilinear sampling, getting lots of bleending darkness from
        //        empty neighboring voxels around illuminated voxels, need to do some sort of dilation in volume texture to combat this
        //     [] dilation for quadrilinear sampling

        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "Voxel Based Rendering Experiement D",
            [](SceneView& view) {
                auto scene = view.getScene();
                auto render = view.getRender();
                const auto& viewState = view.getState();

                voxelizeScene(view);

                {
                    GPU_DEBUG_SCOPE(updateIrradianceCache, "Voxel Based Irradiance Cache")

                    bool bFound = false;
                    auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "BuildIrradianceCache", APP_SHADER_PATH "build_irradiance_cache_c.glsl");
                    auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                    const auto& desc = s_voxelGridIrradiance->getDesc();
                    i32 workGroupX = desc.width / 8;
                    i32 workGroupY = desc.height / 8;
                    i32 workGroupZ = desc.depth / 8;

                    RenderingUtils::dispatchComputePass(
                        p.get(),
                        [scene, desc, viewState](ComputePipeline* p) {
                            viewState.setShaderParameters(p);

                            auto irradianceTex = dynamic_cast<GLTexture3D*>(s_voxelGridIrradiance.get());
                            glBindImageTexture(0, irradianceTex->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);
                            p->setUniform("u_imageSize", glm::ivec3(desc.width, desc.height, desc.depth));

                            p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                            p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));
                            p->setUniform("u_voxelSize", s_voxelSize);
                            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                            p->setUniform("u_voxelGridResolution", s_voxelGridResolution);
                            p->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());
                            p->setTexture("u_voxelGridNormalTex", s_voxelGridNormal.get());

                            p->setUniform("u_numMipLevels", (i32)s_voxelGridOpacity->getDesc().numMips);
                            p->setUniform("u_maxNumIterations", (i32)maxNumRayMarchingIterations);
                            p->setTexture("u_voxelGridOpacityTex", s_voxelGridOpacity.get());
                            p->setTexture("u_voxelGridNormalTex", s_voxelGridNormal.get());

                            if (scene->m_skyLight != nullptr)
                            {
                                p->setTexture("u_skyCubemapTex", scene->m_skyLight->getReflectionCubemap()->cubemap.get());
                            }

                            if (scene->m_directionalLight != nullptr)
                            {
                                glm::vec3 sunDirection = scene->m_directionalLight->m_direction;
                                p->setUniform("u_sunDirection", sunDirection);
                                glm::vec3 sunColor = scene->m_directionalLight->m_color * scene->m_directionalLight->m_intensity;
                                p->setUniform("u_sunColor", sunColor);
                            }
                        },
                        workGroupX,
                        workGroupY,
                        workGroupZ
                    );
                }

                {
                    GPU_DEBUG_SCOPE(visualizeIrradiance, "Visualize Irradiance Cache")

                        visualizeVoxelGrid(
                            s_voxelGridIrradianceVisualization.get(),
                            view,
                            s_voxelSize,
                            s_voxelGridOpacity.get(),
                            s_voxelGridIrradiance.get(),
                            s_voxelGridResolution,
                            s_voxelGridAABB,
                            s_voxelGridCenter,
                            s_voxelGridExtent,
                            0);
                }

                {
                    GPU_DEBUG_SCOPE(updateIrradianceCache, "Rendering Irradiance ")

                    bool bFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", APP_SHADER_PATH "screen_pass_v.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "RenderIrradiancePS", APP_SHADER_PATH "render_irradiance_p.glsl");
                    auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

                    const auto& desc = s_voxelGridVisA->getDesc();
                    RenderingUtils::renderScreenPass(
                        glm::uvec2(desc.width, desc.height),
                        [](RenderPass& pass) {
                            RenderTarget rt(s_voxelGridVisA.get(), 0);
                            pass.setRenderTarget(rt, 0);
                        },
                        p.get(),
                        [viewState, render](GfxPipeline* p) {
                            viewState.setShaderParameters(p);
                            p->setTexture("u_sceneDepthTex", render->depth());
                            p->setTexture("u_sceneNormalTex", render->normal());
                            p->setTexture("u_voxelGridIrradianceTex", s_voxelGridIrradiance.get());
                            p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                            p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                        }
                    );
                }

                RenderingUtils::drawAABB(render->color(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, s_sceneAABB, glm::vec4(Color::red, 1.f));
                RenderingUtils::drawAABB(render->color(), render->depth(), viewState.viewMatrix, viewState.projectionMatrix, s_voxelGridAABB, glm::vec4(Color::red, 1.f));
            });

            Engine::get()->enqueueFrameGfxTask(
                RenderingStage::kPostRenderToBackBuffer,
                "RenderDebugVisualizations", 
                [this](Frame& frame) {
                    glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                    glm::ivec2 resolution(480, 270);
                    RenderingUtils::renderToBackBuffer(s_voxelGridVisA.get(), Viewport { 0, 0, (i32)windowSize.x, (i32)windowSize.y } );
                    RenderingUtils::renderToBackBuffer(s_voxelGridIrradianceVisualization.get(), Viewport { 0, (i32)windowSize.y - resolution.y, resolution.x, resolution.y } );
                });
    }

    // todo E: exploring probed based irradiance volume with voxel ray marching, something like DDGI
    void VoxelBasedGI::experiementE()
    {
    }

    static void visualizePackedSVO(bool, bool);
    static void visualizeSVO(bool, bool);
    static void raymarchingPackedSVO();
    static void raymarchingSVO();

    static GLuint s_voxelFragmentCounter = -1;
    static GLuint s_allocatedSVONodeCounter = -1;
    static GLuint s_allocatedVoxelCounter = -1;

    static const i32 kMaxNumVoxelFragments = 10 * 1024 * 1024;
    static const i32 kMaxNumSVONode = 64 * 1024 * 1024;

    static i32 s_SVOMaxLevelCount = 0;
    static bool bVisualizeInternalNodes = false;
    static bool bVisualizeLeafNodes = false;
    static std::unique_ptr<GHRWBuffer> s_voxelFragmentBuffer = nullptr;

    // these stuffs are highly experiemental =========
    static std::unique_ptr<GHRWBuffer> s_voxelFragmentPointerBuffer = nullptr;
    static std::unique_ptr<GHTexture3D> s_perLeafVoxelFragmentHeadPointer = nullptr;
    static std::unique_ptr<GHTexture3D> s_perLeafVoxelFragmentLinkedListSemaphore = nullptr;
    // ===============================================

    static std::unique_ptr<GHRWBuffer> s_packedSVONodeBuffer = nullptr; 
    static std::unique_ptr<GHRWBuffer> s_SVONodeBuffer = nullptr; 
    static std::unique_ptr<GHTexture3D> s_SVOAlbedo = nullptr;
    static bool bUseRefactoredVersion = true;

    // todo E: exploring ray marching SVO
    void VoxelBasedGI::experiementF()
    {
        if (bUseRefactoredVersion)
        {
            m_sparseVoxelOctreeDemo->render();
        }
        else
        {
            // visualizePackedSVO(false, true);
            visualizeSVO(false, true);
        }

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPostRenderToBackBuffer,
            "RenderDebugVisualizations", 
            [this](Frame& frame) {
                glm::uvec2 windowSize = GfxModule::get()->getWindowSize();
                RenderingUtils::renderToBackBuffer(m_sparseVoxelOctreeDemo->m_sceneColorResolved.get(), Viewport { 0, 0, (i32)windowSize.x, (i32)windowSize.y } );
            });
    }

    enum class Experiement
    {
        A,
        B,
        C,
        D,
        E,
        F,
        Count
    };
    static Experiement s_activeExperiement = Experiement::F;

    void VoxelBasedGI::render()
    {
        switch (s_activeExperiement)
        {
        case Experiement::A: experiementA(); break;
        case Experiement::B: experiementB(); break;
        case Experiement::C: experiementC(); break;
        case Experiement::D: experiementD(); break;
        case Experiement::E: experiementE(); break;
        case Experiement::F: experiementF(); break;
        default: assert(0); break;
        }
    }

    struct VoxelFragment
    {
        glm::vec4 worldSpacePosition;
        glm::vec4 albedo;
    };

    struct VoxelFragmentPointer
    {
        i32 prev = -1;
        i32 next = -1;
        i32 padding0 = 0;
        i32 padding1 = 0;
    };

    struct SVONode
    {
        glm::ivec4 coord;
        u32 bSubdivide;
        u32 bLeaf;
        u32 childIndex;
        u32 padding;
    };

    // try to pack everything into a ivec2 (64 bits)
    // X10Y10Z10, 2 bits for the msb of level,
    // 2 bits for lsb of level, and then 2 bits for subdivision mask, and leaf mask, and then 28 bits for a "pointer" to child node tile
    struct PackedSVONode
    {
        u32 data0;
        u32 data1;
    };

    // using a more compact node representation
    static void sparseSceneVoxelizationA()
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "Sparse Scene Voxelization",
            [](SceneView& view) {
                auto unpack = [](const PackedSVONode& packed) {
                    SVONode result;

                    // todo: this coord mask is incorrect
                    u32 xMask = ((1 << 10) - 1) << 22;
                    u32 yMask = ((1 << 10) - 1) << 12;
                    u32 zMask = ((1 << 10) - 1) << 2;
                    u32 x = (packed.data0 & xMask) >> 22;
                    u32 y = (packed.data0 & yMask) >> 12;
                    u32 z = (packed.data0 & zMask) >> 2;
                    u32 levelA = packed.data0 & 0x3;
                    u32 levelB = (packed.data1 & ~((1 << 30) - 1)) >> 30;
                    u32 level = (levelA << 2) | levelB;

                    result.coord = glm::ivec4(x, y, z, level);
                    result.bSubdivide = ((1 << 29) & packed.data1) > 0 ? 1 : 0;
                    result.bLeaf = ((1 << 28) & packed.data1) > 0 ? 1 : 0;
                    result.childIndex = ((1 << 28) - 1) & packed.data1;

                    return result;
                };

                auto pack = [](SVONode node) {
                    PackedSVONode result;
                    result.data0 = 0x0;
                    result.data1 = 0x0;

                    result.data0 = (node.coord.x << 22) | (node.coord.y << 12) | (node.coord.z << 2);
                    result.data0 &= ~0x3;
                    result.data0 |= ((0x3 << 2) & node.coord.w) >> 2;

                    result.data1 &= ~(0x3 << 30);
                    result.data1 |= ((0x3 & node.coord.w) << 30);

                    result.data1 &= ~(0x1 << 29);
                    if (node.bSubdivide > 0)
                    {
                        result.data1 |= 0x1 << 29;
                    }

                    result.data1 &= ~(0x1 << 28);
                    if (node.bLeaf > 0)
                    {
                        result.data1 |= 0x1 << 28;
                    }

                    result.data1 &= ~((0x1 << 28) - 1);
                    // clear 4 msb bits
                    node.childIndex &= ~(0xF << 28);
                    result.data1 |= node.childIndex;

                    return result;
                };

                glCreateBuffers(1, &s_voxelFragmentCounter);
                glNamedBufferData(s_voxelFragmentCounter, sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

                glCreateBuffers(1, &s_allocatedSVONodeCounter);
                glNamedBufferData(s_allocatedSVONodeCounter, sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

                const auto& desc = GHTexture2D::Desc::create(1024, 1024, 1, PF_RGB16F);
                GHSampler2D pointSampler2D;
                pointSampler2D.setFilteringModeMin(SAMPLER2D_FM_POINT);
                pointSampler2D.setFilteringModeMag(SAMPLER2D_FM_POINT);

                auto scene = view.getScene();
                auto render = view.getRender();
                const auto& viewState = view.getState();

                const i32 kMaxNumVoxelFragments = 10 * 1024 * 1024;
                const i32 kMaxNumSVONode = 64 * 1024 * 1024;

                s_voxelFragmentBuffer = GHRWBuffer::create(sizeof(VoxelFragment) * kMaxNumVoxelFragments);
                s_packedSVONodeBuffer = GHRWBuffer::create(sizeof(PackedSVONode) * kMaxNumSVONode);

                // 1. sparse scene voxelization using packed node
                {
                    GPU_DEBUG_SCOPE(marker, "Sparse Scene Voxelization")

                    // 1.1 build voxel fragment list
                    {
                        GPU_DEBUG_SCOPE(marker, "Build Voxel Fragment List")

                        for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
                        {
                            auto subMeshInstance = scene->m_staticSubMeshInstances[i];
                            auto subMesh = subMeshInstance.subMesh;
                            glm::vec3 subMeshMin = subMesh->m_min;
                            glm::vec3 subMeshMax = subMesh->m_max;
                            glm::vec3 subMeshMinWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMin, 1.f);
                            glm::vec3 subMeshMaxWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMax, 1.f);
                            s_sceneAABB.bound(subMeshMinWS);
                            s_sceneAABB.bound(subMeshMaxWS);
                        }

                        glm::vec3 center = (s_sceneAABB.pmin + s_sceneAABB.pmax) * .5f;
                        glm::vec3 extent = (s_sceneAABB.pmax - s_sceneAABB.pmin) * .5f;

                        f32 sizeX = extent.x * 2.f / s_voxelGridResolution.x;
                        f32 sizeY = extent.y * 2.f / s_voxelGridResolution.y;
                        f32 sizeZ = extent.z * 2.f / s_voxelGridResolution.z;
                        s_voxelSize = glm::vec3(glm::max(sizeX, sizeY));
                        s_voxelSize = glm::vec3(glm::max(s_voxelSize, sizeZ));

                        s_voxelGridCenter = center;
                        s_voxelGridExtent = s_voxelGridResolution * .5f * s_voxelSize;
                        s_voxelGridAABB.bound(s_voxelGridCenter + s_voxelGridExtent);
                        s_voxelGridAABB.bound(s_voxelGridCenter - s_voxelGridExtent);

                        bool bFound = false;
                        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VoxelizeSceneVS", APP_SHADER_PATH "voxelize_scene_v.glsl");
                        auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VoxelizeSceneGS", APP_SHADER_PATH "voxelize_scene_g.glsl");
                        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "BuildVoxelFragmentListPS", APP_SHADER_PATH "build_voxel_fragment_list_p.glsl");
                        auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

                        glm::vec3 superSampledGridResolution = glm::vec3(1024.f);

                        f32 viewports[] = {
                            0, 0, superSampledGridResolution.z, superSampledGridResolution.y, // +x
                            0, 0, superSampledGridResolution.x, superSampledGridResolution.z, // +y
                            0, 0, superSampledGridResolution.x, superSampledGridResolution.y, // +z
                        };
                        glViewportArrayv(0, 3, viewports);

                        // no need to bind framebuffer here since we are not drawing to one
                        auto gfxc = GfxContext::get();
                        gfxc->disableDepthTest();
                        gfxc->disableBlending();

                        p->bind();

                        // important stuffs here
                        s_voxelFragmentBuffer->bind();
                        // clear counter
                        u32 counter = 0;
                        glNamedBufferSubData(s_voxelFragmentCounter, 0, sizeof(u32), &counter);
                        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, s_voxelFragmentCounter);
                        p->setUniform("u_maxFragmentCount", kMaxNumVoxelFragments);

                        p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                        p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                        p->setUniform("u_voxelGridResolution", s_voxelGridResolution);

                        p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                        p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));

                        glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
                        glDisable(GL_CULL_FACE);
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
                        glEnable(GL_CULL_FACE);
                        glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
                        p->unbind();
                    }

                    u32 numVoxelFragments = 0;
                    glGetNamedBufferSubData(s_voxelFragmentCounter, 0, sizeof(u32), &numVoxelFragments);

                    // 1.1.1 debug visualize voxel fragments
                    {
                        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

                        const auto& desc = render->color()->getDesc();
                        RenderPass pass(desc.width, desc.height);
                        RenderTarget rt(render->color(), 0, false);
                        DepthTarget dt(render->depth(), 0, false);
                        pass.setRenderTarget(rt, 0);
                        pass.setDepthTarget(dt);
                        pass.setRenderFunc([numVoxelFragments, viewState](GfxContext* ctx) {
                            bool bFound = false;
                            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeVoxelFragmentsVS", APP_SHADER_PATH "visualize_voxel_fragments_v.glsl");
                            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeVoxelFragmentsPS", APP_SHADER_PATH "visualize_voxel_fragments_p.glsl");
                            auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);
                            p->bind();
                            viewState.setShaderParameters(p.get());
                            s_voxelFragmentBuffer->bind();
                            glBindVertexArray(dummyVAO);
                            glDrawArrays(GL_POINTS, 0, numVoxelFragments);
                            glBindVertexArray(0);
                            });
                            pass.enableDepthTest();
                            pass.render(GfxContext::get());
                    }

                    // 1.2 iteratively subdivide SVO until bottom most level
                    {
                        // initialize root node
                        {
                            PackedSVONode rootNode;
                            rootNode.data0 = 0x0;
                            rootNode.data1 = 0x0;
                            s_packedSVONodeBuffer->write(&rootNode, 0, 0, sizeof(rootNode));

                            // set the allocated nodes count to 1 to start with
                            u32 allocatedNodes = 1;
                            glNamedBufferSubData(s_allocatedSVONodeCounter, 0, sizeof(u32), &allocatedNodes);
                        }

                        s_SVOMaxLevelCount = glm::log2(s_voxelGridResolution.x) + 1;
                        for (i32 level = 0; level < (s_SVOMaxLevelCount - 1); ++level)
                        {
                            // mark nodes that need to be subdivided
                            {
                                bool bFound = false;
                                auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOPackedMarkSubdivisionCS", APP_SHADER_PATH "svo_packed_mark_subdivision_c.glsl");
                                auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                                const static i32 localWorkGroupSizeX = 64;
                                i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                                RenderingUtils::dispatchComputePass(
                                    p.get(),
                                    [level](ComputePipeline* p) {
                                        s_voxelFragmentBuffer->bind();
                                        s_packedSVONodeBuffer->bind();
                                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                                        p->setUniform("u_SVOAABBMin", s_voxelGridAABB.pmin);
                                        p->setUniform("u_SVOAABBMax", s_voxelGridAABB.pmax);
                                        p->setUniform("u_currentLevel", level);
                                        p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                                    },
                                    workGroupSizeX,
                                    1,
                                    1
                                    );
                            }

                            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

                            // allocate new nodes and initialize them
                            {
                                bool bFound = false;
                                auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOPackedAllocateNodesCS", APP_SHADER_PATH "svo_packed_allocate_nodes_c.glsl");
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
                                    [level](ComputePipeline* p) {
                                        s_packedSVONodeBuffer->bind();
                                        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, s_allocatedSVONodeCounter);

                                        p->setUniform("u_SVONodePoolSize", kMaxNumSVONode);
                                        p->setUniform("u_currentLevel", level);
                                        p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                                    },
                                    workGroupSizeX,
                                    workGroupSizeY,
                                    workGroupSizeZ
                                );
                            }

                            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
                        }
                    }
                }
            });
    }

    static void visualizePackedSVO(bool bVisualizeInternalNodes, bool bVisualizeLeafNodes)
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "VisualizeSVO",
            [bVisualizeInternalNodes, bVisualizeLeafNodes](SceneView& view) {
                auto render = view.getRender();
                const auto& desc = render->color()->getDesc();
                const auto& viewState = view.getState();

                i32 numAllocatedNodes = 0;
                glGetNamedBufferSubData(s_allocatedSVONodeCounter, 0, sizeof(u32), &numAllocatedNodes);

                // first pass drawing all the internal nodes
                if (bVisualizeInternalNodes)
                {
                    bool outFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "VisualizePackedSVOVS", APP_SHADER_PATH "visualize_packed_svo_v.glsl");
                    auto gs = ShaderManager::findOrCreateShader<GeometryShader>(outFound, "VisualizeSVOGS", APP_SHADER_PATH "visualize_svo_g.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "VisualizeSVOPS", APP_SHADER_PATH "visualize_svo_p.glsl");
                    auto p = ShaderManager::findOrCreateGeometryPipeline(outFound, vs, gs, ps);

                    RenderPass pass(desc.width, desc.height);
                    RenderTarget rt(render->color(), 0, false);
                    DepthTarget dt(render->depth(), false);
                    pass.setRenderTarget(rt, 0);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([p, numAllocatedNodes, viewState](GfxContext* ctx) {
                        p->bind();
                        viewState.setShaderParameters(p.get());
                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                        p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                        glBindVertexArray(dummyVAO);
                        glDrawArrays(GL_POINTS, 0, numAllocatedNodes);
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
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "VisualizePackedSVOLeafVS", APP_SHADER_PATH "visualize_packed_svo_leaf_v.glsl");
                    auto gs = ShaderManager::findOrCreateShader<GeometryShader>(outFound, "VisualizeSVOLeafGS", APP_SHADER_PATH "visualize_svo_leaf_g.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "VisualizeSVOLeafPS", APP_SHADER_PATH "visualize_svo_leaf_p.glsl");
                    auto p = ShaderManager::findOrCreateGeometryPipeline(outFound, vs, gs, ps);

                    RenderPass pass(desc.width, desc.height);
                    RenderTarget rt(render->color(), 0, false);
                    DepthTarget dt(render->depth(), false);
                    pass.setRenderTarget(rt, 0);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([p, numAllocatedNodes, viewState](GfxContext* ctx) {
                        p->bind();
                        viewState.setShaderParameters(p.get());
                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                        p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                        glBindVertexArray(dummyVAO);
                        glDrawArrays(GL_POINTS, 0, numAllocatedNodes);
                        glBindVertexArray(0);
                        p->unbind();
                        });
                    pass.enableDepthTest();
                    pass.disableBackfaceCulling();
                    pass.render(GfxContext::get());
                }
            }
        );
    }

    static void buildPerLeafNodeVoxelFragmentLinkedList()
    {
        s_voxelFragmentPointerBuffer = GHRWBuffer::create(sizeof(VoxelFragmentPointer) * kMaxNumVoxelFragments);

        const i32 kNumSlicesInPool = 16;
        const auto& desc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, kNumSlicesInPool, 1, PF_R32I);
        GHSampler3D pointSampler3D;
        pointSampler3D.setFilteringModeMin(SAMPLER3D_FM_POINT);
        pointSampler3D.setFilteringModeMag(SAMPLER3D_FM_POINT);
        pointSampler3D.setAddressingModeX(SAMPLER3D_AM_CLAMP);
        pointSampler3D.setAddressingModeY(SAMPLER3D_AM_CLAMP);
        pointSampler3D.setAddressingModeZ(SAMPLER3D_AM_CLAMP);
        s_perLeafVoxelFragmentHeadPointer = GHTexture3D::create(desc, pointSampler3D);
        s_perLeafVoxelFragmentLinkedListSemaphore = GHTexture3D::create(desc, pointSampler3D);

        {
            GPU_DEBUG_SCOPE(clearPerLeafSemaphore, "Clear Per SVO Leaf Semaphore");

            bool bFound = false;
            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "ClearPerLeafSemaphoreCS", APP_SHADER_PATH "svo_clear_per_leaf_semaphore_c.glsl");
            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);
            const auto& desc = s_perLeafVoxelFragmentLinkedListSemaphore->getDesc();
            const static i32 localWorkGroupSize = 8;
            i32 workGroupSizeX = glm::ceil((f32)desc.width/ localWorkGroupSize);
            i32 workGroupSizeY = glm::ceil((f32)desc.height / localWorkGroupSize);
            i32 workGroupSizeZ = glm::ceil((f32)desc.depth / localWorkGroupSize);
            RenderingUtils::dispatchComputePass(
                p.get(),
                [desc](ComputePipeline* p) {
                    s_voxelFragmentBuffer->bind();
                    s_SVONodeBuffer->bind();
                    {
                        auto glTexture = dynamic_cast<GLTexture3D*>(s_perLeafVoxelFragmentHeadPointer.get());
                        glBindImageTexture(0, glTexture->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
                    }
                    {
                        auto glTexture = dynamic_cast<GLTexture3D*>(s_perLeafVoxelFragmentLinkedListSemaphore.get());
                        glBindImageTexture(1, glTexture->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
                    }
                    p->setUniform("u_semaphorePoolSize", glm::ivec3(desc.width, desc.height, desc.depth));
                },
                workGroupSizeX,
                workGroupSizeY,
                workGroupSizeZ
            );
        }

        {
            GPU_DEBUG_SCOPE(buildPerLeafVoxelFragmentLinkedList, "Build Per SVO Leaf Voxel Fragment Linked List");

            // launch a compute thread for each voxel fragment, and building that per leaf node(voxel) voxel fragment linked list
            bool bFound = false;
            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOBuildVoxelFragmentLinkedList", APP_SHADER_PATH "svo_build_leaf_node_voxel_fragment_linked_list_c.glsl");
            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);
            const static i32 localWorkGroupSizeX = 64;
            u32 numVoxelFragments = 0;
            glGetNamedBufferSubData(s_voxelFragmentCounter, 0, sizeof(u32), &numVoxelFragments);
            i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);
            RenderingUtils::dispatchComputePass(
                p.get(),
                [](ComputePipeline* p) {
                    s_voxelFragmentBuffer->bind();
                    s_SVONodeBuffer->bind();
                    s_voxelFragmentPointerBuffer->bind();

                    {
                        auto glTexture = dynamic_cast<GLTexture3D*>(s_perLeafVoxelFragmentHeadPointer.get());
                        glBindImageTexture(0, glTexture->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
                    }

                    {
                        auto glTexture = dynamic_cast<GLTexture3D*>(s_perLeafVoxelFragmentLinkedListSemaphore.get());
                        glBindImageTexture(1, glTexture->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
                    }

                    p->setUniform("u_SVOCenter", s_voxelGridCenter);
                    p->setUniform("u_SVOExtent", s_voxelGridExtent);
                    p->setUniform("u_SVOAABBMin", s_voxelGridAABB.pmin);
                    p->setUniform("u_SVOAABBMax", s_voxelGridAABB.pmax);
                    p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                    const auto& desc = s_perLeafVoxelFragmentHeadPointer->getDesc();
                    p->setUniform("u_voxelPoolSize", glm::ivec3(desc.width, desc.height, desc.depth));
                },
                workGroupSizeX,
                    1,
                    1
                );
        }
    }

    // using a less memory efficient but more human readable representation
    static void sparseSceneVoxelizationB()
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "Sparse Scene Voxelization",
            [](SceneView& view) {
                glCreateBuffers(1, &s_voxelFragmentCounter);
                glNamedBufferData(s_voxelFragmentCounter, sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

                glCreateBuffers(1, &s_allocatedSVONodeCounter);
                glNamedBufferData(s_allocatedSVONodeCounter, sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

                glCreateBuffers(1, &s_allocatedVoxelCounter);
                glNamedBufferData(s_allocatedVoxelCounter, sizeof(u32), nullptr, GL_DYNAMIC_DRAW);

                const auto& desc = GHTexture2D::Desc::create(1024, 1024, 1, PF_RGB16F);
                GHSampler2D pointSampler2D;
                pointSampler2D.setFilteringModeMin(SAMPLER2D_FM_POINT);
                pointSampler2D.setFilteringModeMag(SAMPLER2D_FM_POINT);

                auto scene = view.getScene();
                auto render = view.getRender();
                const auto& viewState = view.getState();

                s_voxelFragmentBuffer = GHRWBuffer::create(sizeof(VoxelFragment) * kMaxNumVoxelFragments);
                s_SVONodeBuffer = GHRWBuffer::create(sizeof(SVONode) * kMaxNumSVONode);

                // 1. sparse scene voxelization using packed node
                {
                    GPU_DEBUG_SCOPE(marker, "Sparse Scene Voxelization")

                    // 1.1 build voxel fragment list
                    {
                        GPU_DEBUG_SCOPE(marker, "Build Voxel Fragment List")

                        for (i32 i = 0; i < scene->m_staticSubMeshInstances.size(); ++i)
                        {
                            auto subMeshInstance = scene->m_staticSubMeshInstances[i];
                            auto subMesh = subMeshInstance.subMesh;
                            glm::vec3 subMeshMin = subMesh->m_min;
                            glm::vec3 subMeshMax = subMesh->m_max;
                            glm::vec3 subMeshMinWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMin, 1.f);
                            glm::vec3 subMeshMaxWS = subMeshInstance.localToWorldMatrix * glm::vec4(subMeshMax, 1.f);
                            s_sceneAABB.bound(subMeshMinWS);
                            s_sceneAABB.bound(subMeshMaxWS);
                        }

                        glm::vec3 center = (s_sceneAABB.pmin + s_sceneAABB.pmax) * .5f;
                        glm::vec3 extent = (s_sceneAABB.pmax - s_sceneAABB.pmin) * .5f;

                        f32 sizeX = extent.x * 2.f / s_voxelGridResolution.x;
                        f32 sizeY = extent.y * 2.f / s_voxelGridResolution.y;
                        f32 sizeZ = extent.z * 2.f / s_voxelGridResolution.z;
                        s_voxelSize = glm::vec3(glm::max(sizeX, sizeY));
                        s_voxelSize = glm::vec3(glm::max(s_voxelSize, sizeZ));

                        s_voxelGridCenter = center;
                        s_voxelGridExtent = s_voxelGridResolution * .5f * s_voxelSize;
                        s_voxelGridAABB.bound(s_voxelGridCenter + s_voxelGridExtent);
                        s_voxelGridAABB.bound(s_voxelGridCenter - s_voxelGridExtent);

                        bool bFound = false;
                        auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VoxelizeSceneVS", APP_SHADER_PATH "voxelize_scene_v.glsl");
                        auto gs = ShaderManager::findOrCreateShader<GeometryShader>(bFound, "VoxelizeSceneGS", APP_SHADER_PATH "voxelize_scene_g.glsl");
                        auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "BuildVoxelFragmentListPS", APP_SHADER_PATH "SVO/build_voxel_fragment_list_p.glsl");
                        auto p = ShaderManager::findOrCreateGeometryPipeline(bFound, vs, gs, ps);

                        glm::vec3 superSampledGridResolution = glm::vec3(1024.f);

                        f32 viewports[] = {
                            0, 0, superSampledGridResolution.z, superSampledGridResolution.y, // +x
                            0, 0, superSampledGridResolution.x, superSampledGridResolution.z, // +y
                            0, 0, superSampledGridResolution.x, superSampledGridResolution.y, // +z
                        };
                        glViewportArrayv(0, 3, viewports);

                        // no need to bind framebuffer here since we are not drawing to one
                        auto gfxc = GfxContext::get();
                        gfxc->disableDepthTest();
                        gfxc->disableBlending();

                        p->bind();

                        // important stuffs here
                        s_voxelFragmentBuffer->bind();
                        // clear counter
                        u32 counter = 0;
                        glNamedBufferSubData(s_voxelFragmentCounter, 0, sizeof(u32), &counter);
                        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, s_voxelFragmentCounter);
                        p->setUniform("u_maxFragmentCount", kMaxNumVoxelFragments);

                        p->setUniform("u_voxelGridCenter", s_voxelGridCenter);
                        p->setUniform("u_voxelGridExtent", s_voxelGridExtent);
                        p->setUniform("u_voxelGridResolution", s_voxelGridResolution);

                        p->setUniform("u_voxelGridAABBMin", glm::vec3(s_voxelGridAABB.pmin));
                        p->setUniform("u_voxelGridAABBMax", glm::vec3(s_voxelGridAABB.pmax));

                        glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
                        glDisable(GL_CULL_FACE);
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
                        glEnable(GL_CULL_FACE);
                        glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);
                        p->unbind();
                    }

                    u32 numVoxelFragments = 0;
                    glGetNamedBufferSubData(s_voxelFragmentCounter, 0, sizeof(u32), &numVoxelFragments);

                    // 1.1.1 debug visualize voxel fragments
                    {
                        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

                        const auto& desc = render->color()->getDesc();
                        RenderPass pass(desc.width, desc.height);
                        RenderTarget rt(render->color(), 0, false);
                        DepthTarget dt(render->depth(), 0, false);
                        pass.setRenderTarget(rt, 0);
                        pass.setDepthTarget(dt);
                        pass.setRenderFunc([numVoxelFragments, viewState](GfxContext* ctx) {
                            bool bFound = false;
                            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "VisualizeVoxelFragmentsVS", APP_SHADER_PATH "visualize_voxel_fragments_v.glsl");
                            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "VisualizeVoxelFragmentsPS", APP_SHADER_PATH "visualize_voxel_fragments_p.glsl");
                            auto p = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);
                            p->bind();
                            viewState.setShaderParameters(p.get());
                            s_voxelFragmentBuffer->bind();
                            glBindVertexArray(dummyVAO);
                            glDrawArrays(GL_POINTS, 0, numVoxelFragments);
                            glBindVertexArray(0);
                            });
                            pass.enableDepthTest();
                            pass.render(GfxContext::get());
                    }

                    // 1.2 iteratively subdivide SVO until bottom most level
                    {
                        // initialize root node
                        {
                            SVONode rootNode;
                            rootNode.coord = glm::ivec4(0, 0, 0, 0);
                            rootNode.bSubdivide = 0;
                            rootNode.bLeaf = 0;
                            rootNode.childIndex = 0;
                            rootNode.padding = 0;
                            s_SVONodeBuffer->write(&rootNode, 0, 0, sizeof(rootNode));

                            // set the allocated nodes count to 1 to start with
                            u32 allocatedNodes = 1;
                            glNamedBufferSubData(s_allocatedSVONodeCounter, 0, sizeof(u32), &allocatedNodes);
                        }

                        s_SVOMaxLevelCount = glm::log2(s_voxelGridResolution.x) + 1;
                        for (i32 level = 0; level < s_SVOMaxLevelCount; ++level)
                        {
                            // mark nodes that need to be subdivided
                            {
                                bool bFound = false;
                                auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOMarkSubdivisionCS", APP_SHADER_PATH "SVO/svo_mark_subdivision_c.glsl");
                                auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                                const static i32 localWorkGroupSizeX = 64;
                                i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                                RenderingUtils::dispatchComputePass(
                                    p.get(),
                                    [level](ComputePipeline* p) {
                                        s_voxelFragmentBuffer->bind();
                                        s_SVONodeBuffer->bind();
                                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                                        p->setUniform("u_SVOAABBMin", s_voxelGridAABB.pmin);
                                        p->setUniform("u_SVOAABBMax", s_voxelGridAABB.pmax);
                                        p->setUniform("u_currentLevel", level);
                                        p->setUniform("u_SVONumLevels", s_SVOMaxLevelCount);
                                    },
                                    workGroupSizeX,
                                    1,
                                    1
                                    );
                            }

                            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

                            // allocate new nodes and initialize them
                            {
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
                                    [level](ComputePipeline* p) {
                                        s_SVONodeBuffer->bind();

                                        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, s_allocatedSVONodeCounter);
                                        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, s_allocatedVoxelCounter);

                                        p->setUniform("u_SVONodePoolSize", kMaxNumSVONode);
                                        p->setUniform("u_currentLevel", level);
                                        p->setUniform("u_SVONumLevels", s_SVOMaxLevelCount);
                                    },
                                    workGroupSizeX,
                                    workGroupSizeY,
                                    workGroupSizeZ
                                );
                            }

                            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
                        }
                    }

                    {
                        // buildPerLeafNodeVoxelFragmentLinkedList();

                        // todo: implement writing voxel (leaf node) data
                        const auto desc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, 8, 1, PF_R32F);
                        const auto& sampler3D = GHSampler3D::create();
                        // these are temp resources so they are released after this rendering pass 
                        std::shared_ptr<GHTexture3D> SVOAlbedoR = GHTexture3D::create(desc, sampler3D);
                        std::shared_ptr<GHTexture3D> SVOAlbedoG = GHTexture3D::create(desc, sampler3D);
                        std::shared_ptr<GHTexture3D> SVOAlbedoB = GHTexture3D::create(desc, sampler3D);
                        std::shared_ptr<GHTexture3D> perVoxelFragmentCounter = GHTexture3D::create(desc, sampler3D);

                        // accumulating voxel attributes
                        {
                            bool bFound = false;
                            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOAccumulateVoxelDataCS", APP_SHADER_PATH "svo_accumulate_voxel_data_c.glsl");
                            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                            const static i32 localWorkGroupSizeX = 64;
                            i32 workGroupSizeX = glm::ceil((f32)numVoxelFragments / localWorkGroupSizeX);

                            RenderingUtils::dispatchComputePass(
                                p.get(),
                                [SVOAlbedoR, SVOAlbedoG, SVOAlbedoB, perVoxelFragmentCounter, desc](ComputePipeline* p) {
                                    s_voxelFragmentBuffer->bind();
                                    p->setUniform("u_SVOCenter", s_voxelGridCenter);
                                    p->setUniform("u_SVOExtent", s_voxelGridExtent);
                                    p->setUniform("u_SVOAABBMin", s_voxelGridAABB.pmin);
                                    p->setUniform("u_SVOAABBMax", s_voxelGridAABB.pmax);
                                    p->setUniform("u_SVOMaxLevelCount", s_SVOMaxLevelCount);
                                    p->setUniform("u_voxelPoolSize", glm::ivec3(desc.width, desc.height, desc.depth));

                                    auto albedoR = dynamic_cast<GLTexture3D*>(SVOAlbedoR.get());
                                    glBindImageTexture(0, albedoR->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
                                    auto albedoG = dynamic_cast<GLTexture3D*>(SVOAlbedoG.get());
                                    glBindImageTexture(1, albedoG->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
                                    auto albedoB = dynamic_cast<GLTexture3D*>(SVOAlbedoB.get());
                                    glBindImageTexture(2, albedoB->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
                                    auto counter = dynamic_cast<GLTexture3D*>(perVoxelFragmentCounter.get());
                                    glBindImageTexture(3, counter->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
                                },
                                workGroupSizeX,
                                1,
                                1
                                );
                        }
                        // resolve average voxel attributes
                        {
                            const auto& avgAlbedoDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, 16, 1, PF_RGBA32F);
                            s_SVOAlbedo = GHTexture3D::create(avgAlbedoDesc, sampler3D);

                            // todo: it would be nice if I can put multiple shader content in one file...!
                            bool bFound = false;
                            auto cs = ShaderManager::findOrCreateShader<ComputeShader>(bFound, "SVOResolveVoxelDataCS", APP_SHADER_PATH "svo_resolve_voxel_data_c.glsl");
                            auto p = ShaderManager::findOrCreateComputePipeline(bFound, cs);

                            const static i32 localWorkGroupSizeX = 64;
                            u32 numVoxels = 0;
                            glGetNamedBufferSubData(s_allocatedVoxelCounter, 0, sizeof(numVoxels), & numVoxels);
                            i32 workGroupSizeX = glm::ceil((f32)numVoxels / localWorkGroupSizeX);

                            RenderingUtils::dispatchComputePass(
                                p.get(),
                                [SVOAlbedoR, SVOAlbedoG, SVOAlbedoB, perVoxelFragmentCounter, avgAlbedoDesc](ComputePipeline* p) {
                                    p->setTexture("u_SVOAlbedoR", SVOAlbedoR.get());
                                    p->setTexture("u_SVOAlbedoG", SVOAlbedoG.get());
                                    p->setTexture("u_SVOAlbedoB", SVOAlbedoB.get());
                                    p->setTexture("u_perVoxelFragmentCounter", perVoxelFragmentCounter.get());
                                    p->setUniform("u_voxelPoolSize", glm::ivec3(avgAlbedoDesc.width, avgAlbedoDesc.height, avgAlbedoDesc.depth));
                                    auto avgAlbedo = dynamic_cast<GLTexture3D*>(s_SVOAlbedo.get());
                                    glBindImageTexture(4, avgAlbedo->getName(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
                                },
                                workGroupSizeX,
                                1,
                                1
                                );
                        }
                    }
                }
            });
    }

    static void visualizeSVO(bool bVisualizeInternalNodes, bool bVisualizeLeafNodes)
    {
        Engine::get()->addSceneRenderingTask(
            SceneRenderingStage::kPostLighting,
            "VisualizeSVO",
            [bVisualizeInternalNodes, bVisualizeLeafNodes](SceneView& view) {
                auto render = view.getRender();
                const auto& desc = render->color()->getDesc();
                const auto& viewState = view.getState();

                i32 numAllocatedNodes = 0;
                glGetNamedBufferSubData(s_allocatedSVONodeCounter, 0, sizeof(u32), &numAllocatedNodes);

                // first pass drawing all the internal nodes
                if (bVisualizeInternalNodes)
                {
                    bool outFound = false;
                    auto vs = ShaderManager::findOrCreateShader<VertexShader>(outFound, "VisualizeSVOVS", APP_SHADER_PATH "SVO/visualize_svo_v.glsl");
                    auto gs = ShaderManager::findOrCreateShader<GeometryShader>(outFound, "VisualizeSVOGS", APP_SHADER_PATH "SVO/visualize_svo_g.glsl");
                    auto ps = ShaderManager::findOrCreateShader<PixelShader>(outFound, "VisualizeSVOPS", APP_SHADER_PATH "SVO/visualize_svo_p.glsl");
                    auto p = ShaderManager::findOrCreateGeometryPipeline(outFound, vs, gs, ps);

                    RenderPass pass(desc.width, desc.height);
                    RenderTarget rt(render->color(), 0, false);
                    DepthTarget dt(render->depth(), false);
                    pass.setRenderTarget(rt, 0);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([p, numAllocatedNodes, viewState](GfxContext* ctx) {
                        p->bind();
                        viewState.setShaderParameters(p.get());
                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                        p->setUniform("u_SVONumLevels", s_SVOMaxLevelCount);
                        glBindVertexArray(dummyVAO);
                        glDrawArrays(GL_POINTS, 0, numAllocatedNodes);
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
                    RenderTarget rt(render->color(), 0, false);
                    DepthTarget dt(render->depth(), false);
                    pass.setRenderTarget(rt, 0);
                    pass.setDepthTarget(dt);
                    pass.setRenderFunc([p, numAllocatedNodes, viewState](GfxContext* ctx) {
                        p->bind();
                        viewState.setShaderParameters(p.get());
                        p->setUniform("u_SVOCenter", s_voxelGridCenter);
                        p->setUniform("u_SVOExtent", s_voxelGridExtent);
                        p->setUniform("u_SVONumLevels", s_SVOMaxLevelCount);
                        p->setTexture("u_SVOAlbedo", s_SVOAlbedo.get());
                        glBindVertexArray(dummyVAO);
                        glDrawArrays(GL_POINTS, 0, numAllocatedNodes);
                        glBindVertexArray(0);
                        p->unbind();
                        });
                    pass.enableDepthTest();
                    pass.disableBackfaceCulling();
                    pass.render(GfxContext::get());
                }
            }
        );
    }

    void VoxelBasedGI::customInitialize(World* world)
    {
        // const char* sceneFilePath = APP_ASSET_PATH "Scene.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "Sponza.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "ConferenceRoom.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "CyberpunkStreet.glb";
        const char* sceneFilePath = APP_ASSET_PATH "PicaPica.glb";

        world->import(sceneFilePath);
        const char* debugConeFilePath = APP_ASSET_PATH "DebugCone.glb";
        AssetManager::import(debugConeFilePath);

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "InitGfxResources",
            [this](Frame& frame) {
                // todo: hack! using this to work around flaws in mesh loading for now
                StaticMesh* debugConeMesh = AssetManager::findAsset<StaticMesh>("InvertedCone");
                if (debugConeMesh != nullptr)
                {
                    for (i32 i = 0; i < debugConeMesh->numSubMeshes(); ++i)
                    {
                        StaticSubMesh* sm = (*debugConeMesh)[i];
                        GfxStaticSubMesh* gfxSm = GfxStaticSubMesh::create(sm->getName(), sm->getGeometry());
                    }
                }

                // todo: this is a temp work around for calling gl functions here,
                // it seems glew is not initiailized across the dll boundary. This
                // needs to be removed later.
                GLenum glewErr = glewInit();
                assert(glewErr == GLEW_OK);

                // common initialization
                {
                    glCreateVertexArrays(1, &dummyVAO);

                    const auto& visDesc = GHTexture2D::Desc::create(m_windowWidth, m_windowHeight, 1, PF_RGB16F);
                    GHSampler2D pointSampler2D;
                    s_voxelGridVisA = GHTexture2D::create(visDesc, pointSampler2D);
                    s_voxelGridVisB = GHTexture2D::create(visDesc, pointSampler2D);
                    s_voxelGridVisC = GHTexture2D::create(visDesc, pointSampler2D);
                    s_voxelGridVisD = GHTexture2D::create(visDesc, pointSampler2D);

                    const auto& sceneColorDesc = GHTexture2D::Desc::create(m_windowWidth, m_windowHeight, 1, PF_RGB16F);
                    s_sceneColor = GHTexture2D::create(sceneColorDesc, pointSampler2D);
                    s_prevFrameSceneColor = GHTexture2D::create(sceneColorDesc, pointSampler2D);
                    s_resolvedSceneColor = GHTexture2D::create(sceneColorDesc, pointSampler2D);
#if 0

                    GHSampler3D pointSampler3D;
                    pointSampler3D.setAddressingModeX(SamplerAddressingMode::Clamp);
                    pointSampler3D.setAddressingModeY(SamplerAddressingMode::Clamp);
                    pointSampler3D.setAddressingModeZ(SamplerAddressingMode::Clamp);
                    pointSampler3D.setFilteringModeMin(SAMPLER3D_FM_POINT);
                    pointSampler3D.setFilteringModeMag(SAMPLER3D_FM_POINT);

                    GHSampler3D trilinearSampler;
                    trilinearSampler.setFilteringModeMin(SAMPLER3D_FM_TRILINEAR);
                    trilinearSampler.setFilteringModeMag(SAMPLER3D_FM_TRILINEAR);

                    assert(isPowerOf2(s_voxelGridResolution.x) && isPowerOf2(s_voxelGridResolution.y) && isPowerOf2(s_voxelGridResolution.z));
                    i32 size = glm::min(glm::min(s_voxelGridResolution.x, s_voxelGridResolution.y), s_voxelGridResolution.z);
                    s_numMips = glm::log2(size) + 1;
                    const auto& desc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, s_numMips, PF_R8);
                    s_voxelGridOpacity = GHTexture3D::create(desc, pointSampler3D);

                    GHSampler3D quadrilinearSampler;
                    quadrilinearSampler.setFilteringModeMin(SAMPLER3D_FM_QUADRILINEAR);
                    quadrilinearSampler.setFilteringModeMag(SAMPLER3D_FM_TRILINEAR);

                    s_voxelGridFilteredOpacity = GHTexture3D::create(
                        GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, s_numMips, PF_R8),
                        quadrilinearSampler);

                    // normal
                    const auto& normalOneChannelDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, 1, PF_R32F);
                    s_voxelGridNormalX = GHTexture3D::create(normalOneChannelDesc, pointSampler3D);
                    s_voxelGridNormalY = GHTexture3D::create(normalOneChannelDesc, pointSampler3D);
                    s_voxelGridNormalZ = GHTexture3D::create(normalOneChannelDesc, pointSampler3D);
                    const auto& normalDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, 1, PF_RGBA32F);
                    s_voxelGridNormal = GHTexture3D::create(normalDesc, trilinearSampler);

                    // direct radiance
                    const auto& directRadianceOneChannelDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, 1, PF_R32F);
                    s_voxelGridDirectRadianceR = GHTexture3D::create(directRadianceOneChannelDesc, pointSampler3D);
                    s_voxelGridDirectRadianceG = GHTexture3D::create(directRadianceOneChannelDesc, pointSampler3D);
                    s_voxelGridDirectRadianceB = GHTexture3D::create(directRadianceOneChannelDesc, pointSampler3D);
                    const auto& directRadianceDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, s_numMips, PF_RGBA32F);
                    s_voxelGridDirectRadiance = GHTexture3D::create(directRadianceDesc, quadrilinearSampler);

                    // per voxel fragment counter
                    const auto& counterDesc = GHTexture3D::Desc::create(s_voxelGridResolution.x, s_voxelGridResolution.y, s_voxelGridResolution.z, 1, PF_R32F);
                    s_voxelGridFragmentCounter = GHTexture3D::create(counterDesc, pointSampler3D);
#endif
                }

#if 0
                initExperiementA(m_windowWidth, m_windowHeight);
                initExperiementB(m_windowWidth, m_windowHeight);
                initExperiementC(m_windowWidth, m_windowHeight);
                initExperiementD(m_windowWidth, m_windowHeight);
#endif
            }
        );

        m_sparseVoxelOctreeDemo->initialize();
        // sparseSceneVoxelizationB();
    }
}
