#include <iostream>
#include <fstream>
#include <queue>
#include <functional>

#include "glfw3.h"
#include "gtx/quaternion.hpp"
#include "glm/gtc/integer.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "json.hpp"
#include "glm.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ImGuizmo/ImGuizmo.h"

#include "Common.h"
#include "AssetManager.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"
#include "CyanUI.h"
#include "Ray.h"
#include "RenderableScene.h"
#include "Lights.h"
#include "LightComponents.h"
#include "RayTracingScene.h"
#include "IOSystem.h"

#define GPU_RAYTRACING 0

namespace Cyan {
    Renderer* Singleton<Renderer>::singleton = nullptr;
    static Mesh* fullscreenQuad = nullptr;

    Renderer::IndirectDrawBuffer::IndirectDrawBuffer() {
        glCreateBuffers(1, &buffer);
        glNamedBufferStorage(buffer, sizeInBytes, nullptr, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        data = glMapNamedBufferRange(buffer, 0, sizeInBytes, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
    }
    
    void Renderer::SceneTextures::initialize(const glm::uvec2& inResolution) {
        if (!bInitialized) {
            resolution = inResolution;
            // render target
            renderTarget = createRenderTarget(resolution.x, resolution.y);
            // scene depth normal buffer
            {
                ITextureRenderable::Spec spec = { };
                spec.type = TEX_2D;
                spec.width = resolution.x;
                spec.height = resolution.y;
                spec.pixelFormat = PF_RGB32F;
                ITextureRenderable::Parameter params = { };
                params.magnificationFilter = FM_POINT;
                params.minificationFilter = FM_POINT;
                depth = new Texture2DRenderable("SceneDepthBuffer", spec, params);
                normal = new Texture2DRenderable("SceneNormalBuffer", spec, params);
            }
            // scene color
            {
                ITextureRenderable::Spec spec = { };
                spec.width = resolution.x;
                spec.height = resolution.y;
                spec.type = TEX_2D;
                spec.pixelFormat = PF_RGB16F;
                color = new Texture2DRenderable("SceneColor", spec);
            }
            // post-processed
            {
            }
            bInitialized = true;
        }
        else if (inResolution != resolution) {
        }
    }

    Renderer::Renderer(GfxContext* ctx, u32 windowWidth, u32 windowHeight)
        : Singleton<Renderer>(), 
        m_ctx(ctx),
        m_windowSize(windowWidth, windowHeight),
        m_frameAllocator(1024 * 1024 * 32) {
        m_manyViewGI = std::make_unique<ManyViewGI>(this, m_ctx);
    }

    void Renderer::initialize() {
        m_instantRadiosity.initialize();
        m_manyViewGI->initialize();
    };

    void Renderer::finalize() {
        // imgui clean up
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Renderer::appendToRenderingTab(const std::function<void()>& command) {
        ImGui::Begin("Cyan", nullptr);
        {
            ImGui::BeginTabBar("##Views");
            {
                if (ImGui::BeginTabItem("Rendering"))
                {
                    ImGui::BeginChild("##Rendering Settings", ImVec2(0, 0), true); 
                    {
                        command();
                        ImGui::EndChild();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void Renderer::drawMeshInstance(const SceneRenderable& scene, RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, MeshInstance* meshInstance, i32 transformIndex) {
        Mesh* parent = meshInstance->parent;
        for (u32 i = 0; i < parent->numSubmeshes(); ++i) {
            IMaterial* material = meshInstance->getMaterial(i);
            if (material) {
                RenderTask task = { };
                task.renderTarget = renderTarget;
                task.viewport = viewport;
                task.shader = material->getMaterialShader();
                task.submesh = parent->getSubmesh(i);
                task.renderSetupLambda = [this, &scene, transformIndex, material](RenderTarget* renderTarget, Shader* shader) {
                    material->setShaderMaterialParameters();
                    shader->setUniform("transformIndex", transformIndex);
                };

                submitRenderTask(std::move(task));
            }
        }
    }

    void Renderer::drawMesh(RenderTarget* renderTarget, Viewport viewport, GfxPipelineState pipelineState, Mesh* mesh, Shader* shader, const RenderSetupLambda& perMeshSetupLambda) {
        perMeshSetupLambda(renderTarget, shader);

        for (u32 i = 0; i < mesh->numSubmeshes(); ++i) {
            RenderTask task = { };
            task.renderTarget = renderTarget;
            task.viewport = viewport;
            task.pipelineState = pipelineState;
            task.shader = shader;
            task.submesh = mesh->getSubmesh(i);

            submitRenderTask(std::move(task));
        }
    }

    void Renderer::drawFullscreenQuad(RenderTarget* renderTarget, Shader* shader, RenderSetupLambda&& renderSetupLambda) {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        drawMesh(
            renderTarget,
            { 0, 0, renderTarget->width, renderTarget->height },
            pipelineState,
            AssetManager::getAsset<Mesh>("FullScreenQuadMesh"),
            shader,
            std::move(renderSetupLambda)
            );
    }

    void Renderer::drawScreenQuad(RenderTarget* renderTarget, Viewport viewport, Shader* shader, RenderSetupLambda&& renderSetupLambda) {
        GfxPipelineState pipelineState;
        pipelineState.depth = DepthControl::kDisable;

        drawMesh(
            renderTarget,
            viewport,
            pipelineState,
            AssetManager::getAsset<Mesh>("FullScreenQuadMesh"),
            shader,
            std::move(renderSetupLambda)
            );
    }

    void Renderer::submitRenderTask(RenderTask&& task) {
        // set render target assuming that render target is already properly setup
        m_ctx->setRenderTarget(task.renderTarget);
        m_ctx->setViewport(task.viewport);

        // set shader
        m_ctx->setShader(task.shader);

        // set graphics pipeline state
        m_ctx->setDepthControl(task.pipelineState.depth);
        m_ctx->setPrimitiveType(task.pipelineState.primitiveMode);

        // pre-draw setup
        task.renderSetupLambda(task.renderTarget, task.shader);

        // kick off the draw call
        auto va = task.submesh->getVertexArray();
        m_ctx->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            m_ctx->drawIndex(task.submesh->numIndices());
        }
        else {
            m_ctx->drawIndexAuto(task.submesh->numVertices());
        }
    }

    void Renderer::registerVisualization(const std::string& categoryName, Texture2DRenderable* visualization, bool* toggle) {
        auto entry = visualizationMap.find(categoryName);
        if (entry == visualizationMap.end()) {
            visualizationMap.insert({ categoryName, { VisualizationDesc{ visualization , toggle } } });
        }
        else {
            entry->second.push_back(VisualizationDesc{ visualization, toggle });
        }
    }

    void Renderer::beginRender() {
        // reset frame allocator
        m_frameAllocator.reset();
    }

    void Renderer::render(Scene* scene, const SceneView& sceneView) {
        beginRender();
        {
            // shared render target for this frame
            glm::uvec2 renderResolution = m_windowSize;
            if (m_settings.enableAA) {
                renderResolution = m_windowSize * 2u;
            }
            m_sceneTextures.initialize(renderResolution);

            // convert Scene instance to SceneRenderable instance for rendering
            SceneRenderable renderableScene(scene, sceneView, m_frameAllocator);

            // shadow
            renderShadowMaps(renderableScene);
            // prepass
            renderSceneDepthNormal(renderableScene, m_sceneTextures.renderTarget, m_sceneTextures.depth, m_sceneTextures.normal);
            // global illumination
            // m_manyViewGI->render(m_sceneTextures.renderTarget, renderableScene, m_sceneTexture.depth, m_sceneTextures.color);
            // main scene pass
            renderSceneBatched(renderableScene, m_sceneTextures.renderTarget, m_sceneTextures.color, { });

            // post processing
            // auto bloom = bloom(m_sceneTextures.color);
            if (m_settings.bPostProcessing) {
                composite(sceneView.renderTexture, m_sceneTextures.color, nullptr, m_windowSize);
            }
            // todo: blit offscreen albedo buffer into output render texture
            else {

            }

            if (m_visualization) {
                visualize(sceneView.renderTexture, m_visualization);
            }
        } 
        endRender();
    }

    void Renderer::visualize(Texture2DRenderable* dst, Texture2DRenderable* src) {
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(dst->width, dst->height));
        renderTarget->setColorBuffer(dst, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(.0f, .0f, .0f, 1.f));

        Shader* shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "BlitQuadShader",
            SHADER_SOURCE_PATH "blit_v.glsl",
            SHADER_SOURCE_PATH "blit_p.glsl"
            });

        drawFullscreenQuad(
            renderTarget.get(),
            shader,
            [this, src](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", src);
            });
    }

    void Renderer::renderToScreen(Texture2DRenderable* inTexture) {
        Shader* fullscreenBlitShader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "BlitQuadShader",
            SHADER_SOURCE_PATH "blit_v.glsl",
            SHADER_SOURCE_PATH "blit_p.glsl"
            });

        // final blit to default framebuffer
        drawFullscreenQuad(
            RenderTarget::getDefaultRenderTarget(m_windowSize.x, m_windowSize.y),
            fullscreenBlitShader,
            [this, inTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", inTexture);
            });
    }

    void Renderer::endRender() {
        m_numFrames++;
    }

    void Renderer::renderShadowMaps(SceneRenderable& inScene) {
        // make a copy
        SceneRenderable scene(inScene);
        for (i32 i = 0; i < inScene.directionalLights.size(); ++i) {
            if (inScene.directionalLights[i]->bCastShadow) {
                inScene.directionalLights[i]->renderShadowMap(scene, this);
                if (auto directionalLight = dynamic_cast<CSMDirectionalLight*>(inScene.directionalLights[i])) {
                    (*inScene.directionalLightBuffer)[i] = directionalLight->buildGpuLight();
                }
            }
        }
        // todo: point light
    }

    void Renderer::renderSceneDepthOnly(SceneRenderable& scene, Texture2DRenderable* outDepthTexture) {
        Shader* depthOnlyShader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs, 
            "DepthOnlyShader", 
            SHADER_SOURCE_PATH "depth_only_v.glsl", 
            SHADER_SOURCE_PATH "depth_only_p.glsl",
            });

        std::unique_ptr<RenderTarget> depthRenderTarget(createDepthOnlyRenderTarget(outDepthTexture->width, outDepthTexture->height));
        depthRenderTarget->setDepthBuffer(reinterpret_cast<DepthTexture2D*>(outDepthTexture));
        depthRenderTarget->clear({ { 0u } });
        m_ctx->setRenderTarget(depthRenderTarget.get());
        m_ctx->setViewport({ 0, 0, depthRenderTarget->width, depthRenderTarget->height });
        m_ctx->setShader(depthOnlyShader);
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        submitSceneMultiDrawIndirect(scene);
    }

    void Renderer::renderSceneDepthNormal(SceneRenderable& scene, RenderTarget* outRenderTarget, Texture2DRenderable* outDepthBuffer, Texture2DRenderable* outNormalBuffer) {
        outRenderTarget->setColorBuffer(outDepthBuffer, 0);
        outRenderTarget->setColorBuffer(outNormalBuffer, 1);
        outRenderTarget->setDrawBuffers({ 0, 1 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(1.f));
        outRenderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f));

        Shader* sceneDepthNormalShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "SceneDepthNormalShader", SHADER_SOURCE_PATH "scene_depth_normal_v.glsl", SHADER_SOURCE_PATH "scene_depth_normal_p.glsl" });
        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setShader(sceneDepthNormalShader);
        m_ctx->setDepthControl(DepthControl::kEnable);
        scene.upload();
        submitSceneMultiDrawIndirect(scene);

        addUIRenderCommand([this, outDepthBuffer, outNormalBuffer]() {
            appendToRenderingTab(
                [this, outDepthBuffer, outNormalBuffer]() {
                    if (ImGui::CollapsingHeader("Scene Prepass")) {
                        ImGui::Image((ImTextureID)outDepthBuffer->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::Image((ImTextureID)outNormalBuffer->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));
                    }
                }
            );
        });
    }

    Renderer::SSGITextures Renderer::screenSpaceRayTracing(Texture2DRenderable* sceneDepthTexture, Texture2DRenderable* sceneNormalTexture, const glm::uvec2& renderResolution) {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = renderResolution.x;
        spec.height = renderResolution.y;
        spec.pixelFormat = PF_RGB16F;
        ITextureRenderable::Parameter parameter = { };
        parameter.minificationFilter = FM_POINT;
        parameter.magnificationFilter = FM_POINT;
        parameter.wrap_r = WM_CLAMP;
        parameter.wrap_s = WM_CLAMP;
        parameter.wrap_t = WM_CLAMP;
        static Texture2DRenderable* ssao = new Texture2DRenderable("SceneDepthBuffer", spec, parameter);
        static Texture2DRenderable* ssbn = new Texture2DRenderable("SceneNormalBuffer", spec, parameter);
        static Texture2DRenderable* ssr = new Texture2DRenderable("SceneNormalBuffer", spec, parameter);

        static f32 kRoughness = .2f;
        static Texture2DRenderable* temporalSsaoBuffer[2] = {
            new Texture2DRenderable("TemporalSSAO_0", spec, parameter),
            new Texture2DRenderable("TemporalSSAO_1", spec, parameter),
        };
        static Texture2DRenderable* temporalSsaoCountBuffer[2] = {
            new Texture2DRenderable("TemporalSSAOSampleCount_0", spec, parameter),
            new Texture2DRenderable("TemporalSSAOSampleCount_1", spec, parameter),
        };

        u32 src = m_numFrames > 0 ? (m_numFrames - 1) % 2 : 1;
        u32 dst = m_numFrames % 2;

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(ssao->width, ssao->height));
        renderTarget->setColorBuffer(ssao, 0);
        renderTarget->setColorBuffer(temporalSsaoBuffer[dst], 0);
        renderTarget->setColorBuffer(ssbn, 1);
        renderTarget->setColorBuffer(ssr, 2);
        renderTarget->setColorBuffer(temporalSsaoCountBuffer[dst], 3);
        renderTarget->setDrawBuffers({ 0, 1, 2, 3 });
        renderTarget->clear({ 
            { 0, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 1, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 2, glm::vec4(0.f, 0.f, 0.f, 1.f) },
            { 3, glm::vec4(0.f, 0.f, 0.f, 1.f) },
        });

        Shader* ssrtShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScreenSpaceRayTracingShader", SHADER_SOURCE_PATH "screenspace_raytracing.glsl", SHADER_SOURCE_PATH "screenspace_raytracing.glsl" });
        drawFullscreenQuad(
            renderTarget.get(),
            ssrtShader,
            [this, src, sceneDepthTexture, sceneNormalTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("depthTexture", sceneDepthTexture);
                shader->setTexture("normalTexture", sceneNormalTexture);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2DRenderable>("BlueNoise_1024x1024");
                shader->setTexture("blueNoiseTexture", blueNoiseTexture);
                // ssao
                shader->setUniform("numFrames", m_numFrames);
                if (m_numFrames > 0) {
                    shader->setTexture("temporalSsaoBuffer", temporalSsaoBuffer[src]);
                    shader->setTexture("temporalSsaoCountBuffer", temporalSsaoCountBuffer[src]);
                }
                // ssr
                shader->setUniform("kRoughness", kRoughness);
                auto skybox = AssetManager::getAsset<TextureCubeRenderable>("Skybox");
                shader->setTexture("reflectionProbe", skybox);
            });

        // screen space bent normal

        // screen space diffuse inter-reflection

        // screen space reflection

        addUIRenderCommand([this, dst]() {
            appendToRenderingTab([=]() {
                if (ImGui::CollapsingHeader("SSGI")) {
                    ImGui::Checkbox("Bent Normal", &m_settings.useBentNormal);
                    ImGui::SliderFloat("kRoughness", &kRoughness, 0.f, 1.f);
                    enum class Visualization
                    {
                        kAmbientOcclusion,
                        kBentNormal,
                        kReflection,
                        kCount
                    };
                    static i32 visualization = (i32)Visualization::kAmbientOcclusion;
                    const char* visualizationNames[(i32)Visualization::kCount] = { "Ambient Occlusion", "Bent Normal", "Reflection" };
                    ImVec2 imageSize(320, 180);
                    ImGui::Combo("Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
                    if (visualization == (i32)Visualization::kAmbientOcclusion) {
                        // ImGui::Image((ImTextureID)(ssao->getGpuResource()), ImVec2(480, 270), ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::Image((ImTextureID)(temporalSsaoBuffer[dst]->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                    if (visualization == (i32)Visualization::kBentNormal) {
                        ImGui::Image((ImTextureID)(ssbn->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                    if (visualization == (i32)Visualization::kReflection) {
                        ImGui::Image((ImTextureID)(ssr->getGpuResource()), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                    }
                }
            });
        });

        return { temporalSsaoBuffer[dst], ssbn };
    }

    Texture2DRenderable* Renderer::renderScene(SceneRenderable& scene, const SceneView& sceneView, const glm::uvec2& outputResolution) {
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = outputResolution.x;
        spec.height = outputResolution.y;
        spec.pixelFormat = PF_RGB16F;
        static Texture2DRenderable* outSceneColor = new Texture2DRenderable("SceneColor", spec);

        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->clear({ { 0 } });

        scene.upload();

        // render mesh instances
        u32 transformIndex = 0u;
        for (auto meshInst : scene.meshInstances)
        {
            drawMeshInstance(
                scene,
                renderTarget.get(),
                { 0u, 0u, outSceneColor->width, outSceneColor->height },
                GfxPipelineState(), 
                meshInst,
                transformIndex++
            );
        }

        // render skybox
        if (scene.skybox) { 
            scene.skybox->render(renderTarget.get());
        }

        return outSceneColor;
    }

    void Renderer::submitSceneMultiDrawIndirect(const SceneRenderable& scene) {
        struct IndirectDrawArrayCommand
        {
            u32  count;
            u32  instanceCount;
            u32  first;
            i32  baseInstance;
        };
        // build indirect draw commands
        auto ptr = reinterpret_cast<IndirectDrawArrayCommand*>(indirectDrawBuffer.data);
        for (u32 draw = 0; draw < scene.drawCallBuffer->getNumElements() - 1; ++draw)
        {
            IndirectDrawArrayCommand& command = ptr[draw];
            command.first = 0;
            u32 instance = (*scene.drawCallBuffer)[draw];
            u32 submesh = (*scene.instanceBuffer)[instance].submesh;
            command.count = SceneRenderable::packedGeometry->submeshes[submesh].numIndices;
            command.instanceCount = (*scene.drawCallBuffer)[draw + 1] - (*scene.drawCallBuffer)[draw];
            command.baseInstance = 0;
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer.buffer);
        // dispatch multi draw indirect
        // one sub-drawcall per instance
        u32 drawCount = (u32)scene.drawCallBuffer->getNumElements() - 1;
        glMultiDrawArraysIndirect(GL_TRIANGLES, 0, drawCount, 0);
    }

    void Renderer::renderSceneBatched(SceneRenderable& scene, RenderTarget* outRenderTarget, Texture2DRenderable* outSceneColor, const SSGITextures& SSGIOutput) {
        scene.upload();

        Shader* scenePassShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "ScenePassShader", SHADER_SOURCE_PATH "scene_pass_v.glsl", SHADER_SOURCE_PATH "scene_pass_p.glsl" });
        m_ctx->setShader(scenePassShader);
        outRenderTarget->setColorBuffer(outSceneColor, 0);
        outRenderTarget->setDrawBuffers({ 0 });
        outRenderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f), false);
        m_ctx->setRenderTarget(outRenderTarget);
        m_ctx->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_ctx->setDepthControl(DepthControl::kEnable);

        // setup ssao
        if (SSGIOutput.ao) {
            auto ssao = SSGIOutput.ao->glHandle;
            if (glIsTextureHandleResidentARB(ssao) == GL_FALSE)
            {
                glMakeTextureHandleResidentARB(ssao);
            }
            scenePassShader->setUniform("SSAO", ssao);
            scenePassShader->setUniform("useSSAO", 1.f);
        }
        else {
            scenePassShader->setUniform("useSSAO", 0.f);
        }

        // setup ssbn
        if (SSGIOutput.bentNormal) {
            if (m_settings.useBentNormal) {
                auto ssbn = SSGIOutput.bentNormal->glHandle;
                scenePassShader->setUniform("useBentNormal", 1.f);
                if (glIsTextureHandleResidentARB(ssbn) == GL_FALSE)
                {
                    glMakeTextureHandleResidentARB(ssbn);
                }
                scenePassShader->setUniform("SSBN", ssbn);
            }
            else {
                scenePassShader->setUniform("useBentNormal", 0.f);
            }
        }

        // sky light
        /* note
        * seamless cubemap doesn't work with bindless textures that's accessed using a texture handle,
        * so falling back to normal way of binding textures here.
        */
        scenePassShader->setTexture("skyLight.irradiance", scene.skyLight->irradianceProbe->m_convolvedIrradianceTexture);
        scenePassShader->setTexture("skyLight.reflection", scene.skyLight->reflectionProbe->m_convolvedReflectionTexture);
        auto BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture()->glHandle;
        if (glIsTextureHandleResidentARB(BRDFLookupTexture) == GL_FALSE) {
            glMakeTextureHandleResidentARB(BRDFLookupTexture);
        }
        scenePassShader->setUniform("skyLight.BRDFLookupTexture", BRDFLookupTexture);

        submitSceneMultiDrawIndirect(scene);

        // render skybox
        if (scene.skybox) {
            scene.skybox->render(outRenderTarget);
        }
    }

    /*
    Texture2DRenderable* Renderer::downsample(Texture2DRenderable* inTexture) {
        Shader* downsampleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownsampleShader", SHADER_SOURCE_PATH "bloom_downsample_v.glsl", SHADER_SOURCE_PATH "bloom_downsample_p.glsl" });
        ITextureRenderable::Spec depthSpec = inTexture->getTextureSpec();
        depthSpec.width = max(1, depthSpec.width / 2);
        depthSpec.height = max(1, depthSpec.height / 2);
        auto outTexture = new
        std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(depthSpec.width, depthSpec.height));
        renderTarget->setColorBuffer(, 0);

        submitFullScreenPass(
            renderTarget.get(),
            downsampleShader, 
            [inTexture](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcImage", inTexture);
            });
    }
    */

#if 0
    Renderer::DownsampleChain Renderer::downsample(RenderTexture2D* inTexture, u32 inNumStages)
    {
        u32 numStages = Min(glm::log2(inTexture->spec.width), inNumStages);
        //todo: using a vector here is an obvious overkill
        std::vector<RenderTexture2D*> downsampleChain((u64)numStages + 1);

        downsampleChain[0] = inTexture;
        for (u32 i = 1; i <= numStages; ++i)
        {
            u32 input = i - 1;
            u32 output = i;

            ITextureRenderable::Spec spec = downsampleChain[input]->spec;
            spec.width /= 2;
            spec.height /= 2;
            downsampleChain[output] = m_renderQueue.createTexture2D("DownsamplePass", spec);

            m_renderQueue.addPass(
                "DownsamplePass__",
                [downsampleChain, input, output](RenderQueue& renderQueue, RenderPass* pass) {
                    pass->addInput(downsampleChain[input]);
                    pass->addInput(downsampleChain[output]);
                    pass->addOutput(downsampleChain[output]);
                },
                [this, downsampleChain, input, output]() {
                    Shader* downsampleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomDownsampleShader", SHADER_SOURCE_PATH "bloom_downsample_v.glsl", SHADER_SOURCE_PATH "bloom_downsample_p.glsl" });
                    std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(downsampleChain[output]->spec.width, downsampleChain[output]->spec.height));
                    renderTarget->setColorBuffer(downsampleChain[output]->getTextureResource(), 0);

                    submitFullScreenPass(
                        renderTarget.get(),
                        { { 0 } },
                        downsampleShader, 
                        [downsampleChain, input](RenderTarget* renderTarget, Shader* shader) {
                            shader->setTexture("srcImage", downsampleChain[input]->getTextureResource());
                        });
                }
            );
        }
        return { downsampleChain, numStages + 1 };
    }

    RenderTexture2D* Renderer::bloom(RenderTexture2D* inTexture)
    {
        const u32 numDownsamplePasses = 6u;

        // bloom setup pass
        struct BloomSetupPassData
        {
            RenderTexture2D* input = nullptr;
            RenderTexture2D* output = nullptr;
        } bloomSetupPassData;
        bloomSetupPassData.input = inTexture;
        ITextureRenderable::Spec spec = inTexture->spec;
        spec.width /= 2;
        spec.height /= 2;
        bloomSetupPassData.output = m_renderQueue.createTexture2D("BloomSetupOutput", spec);
        std::shared_ptr<RenderTarget> renderTarget = std::shared_ptr<RenderTarget>(createRenderTarget(bloomSetupPassData.output->spec.width, bloomSetupPassData.output->spec.height));

        m_renderQueue.addPass(
            /*passName=*/"BloomSetupPass",
            [bloomSetupPassData](RenderQueue& renderQueue, RenderPass* pass) {
                pass->addInput(bloomSetupPassData.input);
                pass->addInput(bloomSetupPassData.output);
                pass->addOutput(bloomSetupPassData.output);
            },
            [this, bloomSetupPassData, renderTarget]() {
                Shader* bloomSetupShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomSetupShader", SHADER_SOURCE_PATH "bloom_setup_v.glsl", SHADER_SOURCE_PATH "bloom_setup_p.glsl" });
                // std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(bloomSetupPassData.output->depthSpec.width, bloomSetupPassData.output->depthSpec.height));
                renderTarget->setColorBuffer(bloomSetupPassData.output->getTextureResource(), 0);

                submitFullScreenPass(
                    renderTarget.get(),
                    { { 0u } },
                    bloomSetupShader, 
                    [this, bloomSetupPassData](RenderTarget* renderTarget, Shader* shader) {
                        shader->setTexture("srcTexture", bloomSetupPassData.input->getTextureResource());
                    });
            }
        );

        // downsample
        DownsampleChain downsampleChain = downsample(bloomSetupPassData.output, numDownsamplePasses);

        const u32 kMinGaussianKernelRadius = 2u;
        std::vector<RenderTexture2D*> upscaleChain(downsampleChain.stages.size());
        upscaleChain[0] = downsampleChain.stages[downsampleChain.stages.size() - 1];
        for (u32 i = downsampleChain.stages.size() - 1, input = 0; i > 0; --i, ++input)
        {
            u32 output = input + 1;
            u32 blend = i - 1;

            // Gaussian blur pass
            downsampleChain.stages[blend] = gaussianBlur(downsampleChain.stages[blend], i, (f32)i * .5f);

            // upscale & blend pass
            ITextureRenderable::Spec spec = upscaleChain[input]->spec;
            spec.width = downsampleChain.stages[blend]->spec.width;
            spec.height = downsampleChain.stages[blend]->spec.height;
            upscaleChain[output] = m_renderQueue.createTexture2D("Upscaled", spec);
            m_renderQueue.addPass(
                "Upscale",
                [downsampleChain, &upscaleChain, input, blend, output](RenderQueue& renderQueue, RenderPass* pass) {
                    pass->addInput(downsampleChain.stages[blend]);
                    pass->addInput(upscaleChain[input]);
                    pass->addInput(upscaleChain[output]);
                    pass->addOutput(upscaleChain[output]);
                },
                [this, downsampleChain, blend, upscaleChain, input, output]() {
                    Shader* upscaleShader = ShaderManager::createShader({ ShaderType::kVsPs, "BloomUpscaleShader", SHADER_SOURCE_PATH "bloom_upscale_v.glsl", SHADER_SOURCE_PATH "bloom_upscale_p.glsl" });
                    auto upscaledOutput = upscaleChain[output];
                    std::unique_ptr<RenderTarget> renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(upscaledOutput->spec.width, upscaledOutput->spec.height));
                    renderTarget->setColorBuffer(upscaleChain[output]->getTextureResource(), 0);

                    submitFullScreenPass(
                        renderTarget.get(),
                        { { 0u } },
                        upscaleShader,
                        [this, downsampleChain, upscaleChain, input, blend](RenderTarget* renderTarget, Shader* shader) {
                            shader->setTexture("srcTexture", upscaleChain[input]->getTextureResource())
                                .setTexture("blendTexture", downsampleChain.stages[blend]->getTextureResource());
                        });
                }
            );
        }

        return upscaleChain[upscaleChain.size() - 1];
    }
#endif

    void Renderer::localToneMapping() {
        // step 0: build 3 fusion candidates using 3 different exposure settings
        // step 1: build lightness map of each fusion candidate
        // step 2: build fusion weight map and a according Gaussian pyramid
        // step 3: build Laplacian pyramid for each fusion candidate
    }

    void Renderer::composite(Texture2DRenderable* composited,Texture2DRenderable* inSceneColor, Texture2DRenderable* inBloomColor, const glm::uvec2& outputResolution) {
        // add a reconstruction pass using Gaussian filter
        // todo: this pass is so slow!!!!! disabled at the moment
        // gaussianBlur(inSceneColor, 2u, 1.0f);

        Shader* compositeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "CompositeShader", SHADER_SOURCE_PATH "composite_v.glsl", SHADER_SOURCE_PATH "composite_p.glsl" });
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(composited->width, composited->height));
        renderTarget->setColorBuffer(composited, 0);

        drawFullscreenQuad(
            renderTarget.get(),
            compositeShader,
            [this, inBloomColor, inSceneColor](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("enableTonemapping", m_settings.enableTonemapping ? 1.f : 0.f);
                shader->setUniform("tonemapOperator", m_settings.tonemapOperator);
                shader->setUniform("whitePointLuminance", m_settings.whitePointLuminance);
                shader->setUniform("smoothstepWhitePoint", m_settings.smoothstepWhitePoint);
                if (inBloomColor && m_settings.enableBloom) {
                    shader->setUniform("enableBloom", 1.f);
                } else {
                    shader->setUniform("enableBloom", 0.f);
                }
                shader->setUniform("exposure", m_settings.exposure)
                    .setUniform("colorTempreture", m_settings.colorTempreture)
                    .setUniform("bloomIntensity", m_settings.bloomIntensity)
                    .setTexture("bloomTexture", inBloomColor)
                    .setTexture("sceneColorTexture", inSceneColor);
            });
    }

    // todo: this gaussian blur pass is too slow, takes about 2-3ms
    void Renderer::gaussianBlur(Texture2DRenderable* inoutTexture, u32 inRadius, f32 inSigma) {
        static const u32 kMaxkernelRadius = 10;
        static const u32 kMinKernelRadius = 2;

        struct GaussianKernel
        {
            GaussianKernel(u32 inRadius, f32 inSigma, LinearAllocator& allocator)
                : radius(Max(kMinKernelRadius, Min(inRadius, kMaxkernelRadius))), sigma(inSigma)
            {
                u32 kernelSize = radius;
                // todo: allocate weights from stack or frame allocator instead
                weights = new f32[kMaxkernelRadius];

                // calculate kernel weights
                f32 totalWeight = 0.f;
                const f32 step = 1.f;
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    f32 x = (f32)i * step;
                    weights[i] = calcWeight(x);
                    totalWeight += weights[i];
                }

                // tweak the weights so that the boundry sample weight goes to 0
                f32 correction = calcWeight(step * kernelSize);
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] -= correction;
                    totalWeight -= correction;
                }

                // normalize the weights to 1
                totalWeight = totalWeight * 2.f - weights[0];
                for (u32 i = 0; i < kernelSize; ++i)
                {
                    weights[i] /= totalWeight;
                }
            };

            ~GaussianKernel()
            {
                delete[] weights;
            }

            f32 calcWeight(f32 x)
            {
                f32 coef = 1.f / (sigma * glm::sqrt(2.0f * M_PI));
                return coef * glm::exp(-.5f  * pow(x / sigma, 2.0));
            }

            u32 radius = 0u;
            f32 mean = 0.f;
            f32 sigma = 0.5f;
            f32* weights = nullptr;
        };

        Shader* gaussianBlurShader = ShaderManager::createShader({ ShaderType::kVsPs, "GaussianBlurShader", SHADER_SOURCE_PATH "gaussian_blur_v.glsl", SHADER_SOURCE_PATH "gaussian_blur_p.glsl" });

        // create scratch buffer for storing intermediate output
        ITextureRenderable::Parameter params = { };
        params.wrap_s = WM_CLAMP;
        params.wrap_r = WM_CLAMP;
        params.wrap_t = WM_CLAMP;
        auto scratchBuffer = std::make_shared<Texture2DRenderable>("GaussianBlurH", inoutTexture->getTextureSpec(), params);
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(inoutTexture->width, inoutTexture->height));
        renderTarget->setColorBuffer(scratchBuffer.get(), 0);

        GaussianKernel kernel(inRadius, inSigma, m_frameAllocator);

        // horizontal pass
        drawFullscreenQuad(
            renderTarget.get(),
            gaussianBlurShader,
            [inoutTexture, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", inoutTexture);
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 1.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );

        // vertical pass
        renderTarget->setColorBuffer(inoutTexture, 0);
        // execute
        drawFullscreenQuad(
            renderTarget.get(),
            gaussianBlurShader,
            [scratchBuffer, &kernel](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcTexture", scratchBuffer.get());
                shader->setUniform("kernelRadius", kernel.radius);
                shader->setUniform("pass", 0.f);
                for (u32 i = 0; i < kMaxkernelRadius; ++i)
                {
                    char name[32] = { };
                    sprintf_s(name, "weights[%d]", i);
                    shader->setUniform(name, kernel.weights[i]);
                }
            }
        );
    }

    void Renderer::addUIRenderCommand(const std::function<void()>& UIRenderCommand) {
        m_UIRenderCommandQueue.push(UIRenderCommand);
    }

    void Renderer::renderUI() {
        // set to default render target
        m_ctx->setRenderTarget(nullptr, { });

        // begin imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        while (!m_UIRenderCommandQueue.empty())
        {
            const auto& command = m_UIRenderCommandQueue.front();
            command();
            m_UIRenderCommandQueue.pop();
        }

        // end imgui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // todo: refactor this, this should really just be renderScene()
    void Renderer::renderSceneToLightProbe(Scene* scene, LightProbe* probe, RenderTarget* renderTarget) {
#if 0 
        // updateTransforms(scene);

        // only capture static objects
        std::vector<Entity*> staticObjects;
        for (u32 i = 0; i < scene->entities.size(); ++i)
        {
            if (scene->entities[i]->m_static)
            {
                staticObjects.push_back(scene->entities[i]);
            }
        }

        // render sun light shadow
        renderSunShadow(scene, staticObjects);

        // turn off ssao(
        m_settings.enableSSAO = false;

        // render scene into each face of the cubemap
        Camera camera = { };
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        camera.n = 0.1f;
        camera.f = 100.f;
        camera.fov = glm::radians(90.f);

        for (u32 f = 0; f < (sizeof(LightProbeCameras::cameraFacingDirections)/sizeof(LightProbeCameras::cameraFacingDirections[0])); ++f)
        {
            camera.position = probe->position;
            camera.lookAt = camera.position + LightProbeCameras::cameraFacingDirections[f];
            camera.worldUp = LightProbeCameras::worldUps[f];
            camera.update();

            // updateFrameGlobalData(scene, camera);
            // draw skybox
            if (scene->m_skybox)
            {
                scene->m_skybox->render();
            }
            renderTarget->clear({ { (i32)f } });
            // draw entities 
            for (u32 i = 0; i < staticObjects.size(); ++i)
            {
                drawEntity(
                    renderTarget,
                    { { (i32)f}, {}, {}, {} },
                    false,
                    { 0u, 0u, probe->sceneCapture->width, probe->sceneCapture->height }, 
                    GfxPipelineState(), 
                    staticObjects[i]);
            }
        }

        m_settings.enableSSAO = true;
#endif
    }

    void Renderer::debugDrawSphere(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
        drawMesh(
            renderTarget,
            viewport,
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("Sphere"),
            debugDrawShader,
            [this, position, scale, view, projection](RenderTarget* renderTarget, Shader* shader) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                shader->setUniform("mvp", mvp);
                shader->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeImmediate(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::mat4& view, const glm::mat4& projection) {
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawImmediateShader", SHADER_SOURCE_PATH "debug_draw_immediate_v.glsl", SHADER_SOURCE_PATH "debug_draw_immediate_p.glsl" });
        drawMesh(
            renderTarget,
            viewport,
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            debugDrawShader,
            [this, position, scale, view, projection](RenderTarget* renderTarget, Shader* shader) {
                glm::mat4 mvp(1.f);
                mvp = glm::translate(mvp, position);
                mvp = glm::scale(mvp, scale);
                mvp = projection * view * mvp;
                shader->setUniform("mvp", mvp);
                shader->setUniform("color", glm::vec3(1.f, 0.f, 0.f));
            }
        );
    }

    void Renderer::debugDrawCubeBatched(RenderTarget* renderTarget, const Viewport& viewport, const glm::vec3& position, const glm::vec3& scale, const glm::vec3& facingDir, const glm::vec4& albedo, const glm::mat4& view, const glm::mat4& projection) {

    }

    void Renderer::debugDrawLineImmediate(const glm::vec3& v0, const glm::vec3& v1) {

    }

    void Renderer::debugDrawCubemap(TextureCubeRenderable* cubemap) {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = 320;
        spec.height = 180;
        spec.pixelFormat = PF_RGB16F;

        static Texture2DRenderable* outTexture = new Texture2DRenderable("CubemapViewerTexture", spec);
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        Shader* shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, 
                "DebugDrawCubemapShader", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_v.glsl", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_p.glsl" 
        });
        glDisable(GL_CULL_FACE);
        drawMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            shader,
            [cubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("model", glm::mat4(1.f));
                shader->setUniform("view", camera.view());
                shader->setUniform("projection", camera.projection());
                shader->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }

    void Renderer::debugDrawCubemap(GLuint cubemap) {
        static PerspectiveCamera camera(
            glm::vec3(0.f, 1.f, 2.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f),
            90.f,
            0.1f,
            16.f,
            16.f / 9.f
        );

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(320, 180));
        ITextureRenderable::Spec spec = { };
        spec.type = TEX_2D;
        spec.width = 320;
        spec.height = 180;
        spec.pixelFormat = PF_RGB16F;

        static Texture2DRenderable* outTexture = new Texture2DRenderable("CubemapViewerTexture", spec);
        renderTarget->setColorBuffer(outTexture, 0);
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2, 0.2, 0.2, 1.f));
        
        Shader* shader = ShaderManager::createShader({ 
                ShaderSource::Type::kVsPs, 
                "DebugDrawCubemapShader", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_v.glsl", 
                SHADER_SOURCE_PATH "debug_draw_cubemap_p.glsl" 
        });
        glDisable(GL_CULL_FACE);
        drawMesh(
            renderTarget.get(),
            {0, 0, renderTarget->width, renderTarget->height },
            GfxPipelineState(),
            AssetManager::getAsset<Mesh>("UnitCubeMesh"),
            shader,
            [cubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("model", glm::mat4(1.f));
                shader->setUniform("view", camera.view());
                shader->setUniform("projection", camera.projection());
                shader->setUniform("cubemap", 128);
                glBindTextureUnit(128, cubemap);
                // shader->setTexture("cubemap", cubemap);
            }
        );
        glEnable(GL_CULL_FACE);

        // draw the output texture to the cubemap viewer
        addUIRenderCommand([]() {
            ImGui::Begin("Debug Viewer"); {

                ImGui::Image((ImTextureID)outTexture->getGpuResource(), ImVec2(320, 180), ImVec2(0, 1), ImVec2(1, 0));

                // todo: implement a proper mouse input mechanism for this viewer
                // todo: abstract this into a debug viewer widget or something
                ImVec2 rectMin = ImGui::GetWindowContentRegionMin();
                ImVec2 rectMax = ImGui::GetWindowContentRegionMax();
                if (ImGui::IsMouseHoveringRect(rectMin, rectMax)) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        glm::dvec2 mouseCursorChange = IOSystem::get()->getMouseCursorChange();
                        const f32 C = 0.02f;
                        f32 phi = mouseCursorChange.x * C;
                        f32 theta = mouseCursorChange.y * C;
                        /** note - @min:
                        * copied from Camera.cpp CameraEntity::orbit();
                        */
                        glm::vec3 p = camera.position - camera.lookAt;
                        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.worldUp);
                        quat = glm::rotate(quat, -theta, camera.right());
                        glm::mat4 model(1.f);
                        model = glm::translate(model, camera.lookAt);
                        glm::mat4 rot = glm::toMat4(quat);
                        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
                        camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
                    }
                }
            } ImGui::End();
        });
    }
}