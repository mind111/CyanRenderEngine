#include "RenderingUtils.h"
#include "SSGI.h"
#include "RenderPass.h"
#include "SceneRender.h"
#include "Shader.h"
#include "RenderTexture.h"
#include "Lights.h"

namespace Cyan
{
    SSGIRenderer::SSGIRenderer()
    {
        {
            auto& noiseTextures = RenderingUtils::get()->getNoiseTextures();
            m_blueNoise_16x16_R[0] = noiseTextures.blueNoise16x16_R_0.get();
            m_blueNoise_16x16_R[1] = noiseTextures.blueNoise16x16_R_1.get();
            m_blueNoise_16x16_R[2] = noiseTextures.blueNoise16x16_R_2.get();
            m_blueNoise_16x16_R[3] = noiseTextures.blueNoise16x16_R_3.get();
            m_blueNoise_16x16_R[4] = noiseTextures.blueNoise16x16_R_4.get();
            m_blueNoise_16x16_R[5] = noiseTextures.blueNoise16x16_R_5.get();
            m_blueNoise_16x16_R[6] = noiseTextures.blueNoise16x16_R_6.get();
            m_blueNoise_16x16_R[7] = noiseTextures.blueNoise16x16_R_7.get();
        }

        // m_debugger = std::make_unique<SSGIDebugger>(this);
    }

    SSGIRenderer::~SSGIRenderer()
    {
    }

    void SSGIRenderer::renderSceneIndirectLighting(Scene* scene, SceneRender* render, const SceneView::State& viewState)
    {
        renderAO(render, viewState);
#if 0
        renderDiffuseGI(scene->m_skyLight, render, viewState);

        // compose final indirect lighting
        {
            GPU_DEBUG_SCOPE(SSGIComposeIndirectLighting, "SSGIComposeIndirectLighting");

            auto renderer = Renderer::get();
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "SSGIIndirectLightingPS", SHADER_SOURCE_PATH "SSGI_indirect_lighting_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIIndirectLighting", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectLighting()),
                [render](RenderPass& pass) {
                    pass.setRenderTarget(render->indirectLighting(), 0, /*bClear=*/false);
                },
                pipeline,
                [this, scene, render, viewState](ProgramPipeline* p) {
                    viewState.setShaderParameters(p);
                    p->setTexture("sceneDepth", render->depth());
                    p->setTexture("sceneNormal", render->normal());
                    p->setTexture("sceneAlbedo", render->albedo());
                    p->setTexture("sceneMetallicRoughness", render->metallicRoughness());

                    p->setUniform("settings.bNearFieldSSAO", m_settings.bNearFieldSSAO ? 1.f : 0.f);
                    p->setTexture("SSGI_NearFieldSSAO", render->ao());
                    p->setUniform("settings.bIndirectIrradiance", m_settings.bIndirectIrradiance ? 1.f : 0.f);
                    p->setTexture("SSGI_Diffuse", render->indirectIrradiance());
                    p->setUniform("settings.indirectBoost", m_settings.indirectBoost);
                }
            );
        }
#endif
    }

    void SSGIRenderer::renderAO(SceneRender* render, const SceneView::State& viewState)
    {
        auto outAO = render->ao();
        GHTexture2D::Desc desc = outAO->getDesc();
        RenderTexture2D aoTemporalOutput("AOTemporalOutput", desc);

        // temporal pass: taking 1 new ao sample per pixel and does temporal filtering 
        {
            GPU_DEBUG_SCOPE(SSAOSampling, "SSAOTemporal");

            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SSGIAOPS", ENGINE_SHADER_PATH "ssgi_ao_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            auto outAO = render->ao();
            auto& desc = outAO->getDesc();

            // temporal pass
            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [aoTemporalOutput](RenderPass& rp) {
                    RenderTarget aoRenderTarget(aoTemporalOutput.getGHTexture2D());
                    aoRenderTarget.bNeedsClear = true;
                    aoRenderTarget.clearColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
                    rp.setRenderTarget(aoRenderTarget, 0);
                },
                gfxp.get(),
                [this, render, viewState](GfxPipeline* p) {
                    viewState.setShaderParameters(p);
                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());
                    p->setTexture("sceneNormalBuffer", render->normal());
                    p->setTexture("AOHistoryBuffer", render->aoHistory());
                    p->setTexture("blueNoise_16x16_R[0]", m_blueNoise_16x16_R[0]);
                    p->setTexture("blueNoise_16x16_R[1]", m_blueNoise_16x16_R[1]);
                    p->setTexture("blueNoise_16x16_R[2]", m_blueNoise_16x16_R[2]);
                    p->setTexture("blueNoise_16x16_R[3]", m_blueNoise_16x16_R[3]);
                    p->setTexture("blueNoise_16x16_R[4]", m_blueNoise_16x16_R[4]);
                    p->setTexture("blueNoise_16x16_R[5]", m_blueNoise_16x16_R[5]);
                    p->setTexture("blueNoise_16x16_R[6]", m_blueNoise_16x16_R[6]);
                    p->setTexture("blueNoise_16x16_R[7]", m_blueNoise_16x16_R[7]);

                    // todo: once sampler object is properly implemented, recover the following code
    #if 0
                    if (frameCount > 0)
                    {
                        p->setUniform("prevFrameViewMatrix", prevFrameView);
                        p->setUniform("prevFrameProjection", prevFrameProjection);

                        glBindSampler(32, depthBilinearSampler);
                        p->setUniform("prevFrameSceneDepthTexture", 32);
                        glBindTextureUnit(32, prevSceneDepth.getGfxTexture2D()->getGpuResource());

                        glBindSampler(33, SSAOHistoryBilinearSampler);
                        p->setUniform("AOHistoryBuffer", 33);
                        glBindTextureUnit(33, AOHistoryBuffer.getGfxTexture2D()->getGpuResource());

                        // p->setTexture("prevFrameSceneDepthTexture", prevSceneDepth);
                        p->setTexture("AOHistoryBuffer", AOHistoryBuffer.getGfxTexture2D());
                    }
    #endif
                });
        }

#if 0
        // spatial pass that applies bilateral filtering
        {
            GPU_DEBUG_SCOPE(SSAOBilaterial, "SSAOBilateralFiltering");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "SSAOBilateralPS", SHADER_SOURCE_PATH "ssgi_ao_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOBilateral", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->ao()),
                [render](RenderPass& pass) {
                    RenderTarget aoRenderTarget(render->ao(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, outAO, aoTemporalOutput, render, viewState](ProgramPipeline* p) {
                    viewState.setShaderParameters(p);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("aoTemporalOutput", aoTemporalOutput.getGfxTexture2D());
                }
            );
        }

        // todo: could potentially feed spatial output to history buffer
        renderer->blitTexture(render->aoHistory(), aoTemporalOutput.getGfxTexture2D());
#endif
    }

#if 0
    void SSGIRenderer::renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(SSGIDiffuse, "SSGIDiffuse");

        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
        CreatePS(ps, "SSGIStochasticTemporal", SHADER_SOURCE_PATH "ssgi_bruteforce_temporal_p.glsl");
        CreatePixelPipeline(p, "SSGIBruteforceTemporal", vs, ps);

        // temporal pass
        {
            auto outIndirectIrradiance = render->indirectIrradiance();
            RenderTexture2D temporalIndirectIrradianceBuffer("SSGITemporalIndirectIrradianceBuffer", outIndirectIrradiance->getSpec());

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render](RenderPass& pass) {
                    RenderTarget renderTarget(render->indirectIrradiance(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                p,
                [this, skyLight, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    auto hiZ = render->hiZ();
                    p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
                    p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
                    p->setUniform("settings.kTraceStopMipLevel", (i32)m_settings.kTracingStopMipLevel);
                    p->setUniform("settings.kMaxNumIterationsPerRay", (i32)m_settings.kMaxNumIterationsPerRay);
                    p->setUniform("settings.bSSDO", m_settings.bSSDO ? 1.f : 0.f);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("sceneNormalBuffer", render->normal()); 
                    p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
                    p->setTexture("skyCubemap", skyLight->m_cubemap.get());

                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());
                    p->setTexture("prevFrameIndirectIrradianceBuffer", render->prevFrameIndirectIrradiance());
                }
            );
        }
        // spatial pass
        {
        }
    }

    void SSGIRenderer::renderReflection(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
    }

    void SSGIRenderer::debugDraw(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        if (bDebugging)
        {
            m_debugger->render(render, viewParameters);
        }
    }

    void SSGIRenderer::renderUI()
    {
        if (ImGui::TreeNode("SSGI"))
        {
            ImGui::Text("SSDO"); ImGui::SameLine();
            ImGui::Checkbox("##SSDO", &m_settings.bSSDO);
            ImGui::SliderFloat("##Indirect Boost", &m_settings.indirectBoost, Settings::kMinIndirectBoost, Settings::kMaxIndirectBoost);
            ImGui::Text("Debugging"); ImGui::SameLine();
            ImGui::Checkbox("##Debugging", &bDebugging); 
            if (bDebugging)
            {
                m_debugger->renderUI();
            }
            ImGui::TreePop();
        }
    }

    SSGIDebugger::SSGIDebugger(SSGIRenderer * renderer)
        : m_SSGIRenderer(renderer)
    {
        m_rayMarchingInfos.resize(m_SSGIRenderer->m_settings.kMaxNumIterationsPerRay);
        for (i32 iter = 0; iter < m_SSGIRenderer->m_settings.kMaxNumIterationsPerRay; ++iter)
        {
            m_rayMarchingInfos[iter] = RayMarchingInfo{ };
        }
    }

    SSGIDebugger::~SSGIDebugger()
    {

    }

    void SSGIDebugger::render(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(SSGIDebugRender, "DebugDrawSSGIRay")

        debugDrawRay(render, viewParameters);
    }

    void SSGIDebugger::renderUI()
    {
        if (ImGui::TreeNode("SSGI Debugger"))
        {
            ImGui::Text("Freeze Debug Ray"); ImGui::SameLine();
            ImGui::Checkbox("##Freeze Debug Ray", &m_freezeDebugRay);

            if (!m_freezeDebugRay)
            {
                f32 debugRayScreenCoord[2] = { m_debugRayScreenCoord.x, m_debugRayScreenCoord.y };
                ImGui::Text("Debug Ray ScreenCoord"); ImGui::SameLine();
                ImGui::SliderFloat2("##Debug Ray ScreenCoord", debugRayScreenCoord, 0.f, 1.f);
                m_debugRayScreenCoord.x = debugRayScreenCoord[0];
                m_debugRayScreenCoord.y = debugRayScreenCoord[1];

                f32 depthSampleCoord[2] = { m_depthSampleCoord.x, m_depthSampleCoord.y };
                ImGui::Text("Debug DepthSample Coord"); ImGui::SameLine();
                ImGui::SliderFloat2("##Debug DepthSample Coord", depthSampleCoord, 0.f, 1.f);
                m_depthSampleCoord.x = depthSampleCoord[0];
                m_depthSampleCoord.y = depthSampleCoord[1];

                ImGui::Text("Debug DepthSample Mip"); ImGui::SameLine();
                ImGui::SliderInt("##Debug DepthSample Mip", &m_depthSampleLevel, 0, 10);
                ImGui::Text("Scene Depth: %.5f", m_depthSample.sceneDepth.x);
                ImGui::Text("Quadtree MinDepth: %.5f", m_depthSample.quadtreeMinDepth.x);
            }
            ImGui::TreePop();
        }
    }

    void drawQuadtreeCell(GfxTexture2D* outColor, GfxDepthTexture2D* depth, HierarchicalZBuffer* hiz, const SceneCamera::ViewParameters& viewParameters, i32 mipLevel, glm::vec2 cellCenter, f32 cellSize)
    {
        CreateVS(vs, "DebugDrawQuadtreeCellVS", SHADER_SOURCE_PATH "debug_draw_quadtree_cell_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawQuadtreeCell", vs, ps);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depth, /*bClearDepth=*/false);
        pass.drawLambda = [=](GfxContext* ctx) {
            p->bind(ctx);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            viewParameters.setShaderParameters(p);
            p->setTexture("depthQuadtree", hiz->m_depthBuffer.get());
            p->setUniform("mipLevel", mipLevel);
            p->setUniform("cellCenter", cellCenter);
            p->setUniform("cellSize", cellSize);
            p->setUniform("cellColor", vec4ToVec3(Color::blue));
            p->setUniform("cameraView", viewParameters.viewMatrix);
            p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            glDrawArrays(GL_POINTS, 0, 1);
            va->unbind();
            p->unbind(ctx);
        };
        auto renderer = Renderer::get();
        pass.render(renderer->getGfxCtx());
    }

    void SSGIDebugger::debugDrawRay(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(debugDrawSSGIRay, "DebugDrawSSGIRay")

        const auto& SSGISettings = m_SSGIRenderer->m_settings;

        // generate a ray data
        if (!m_freezeDebugRay)
        {
            if (m_debugRayBuffer == nullptr)
            {
                m_debugRayBuffer = std::make_unique<ShaderStorageBuffer>("DebugRayBuffer", sizeof(DebugRay));
            }
            if (m_rayMarchingInfoBuffer == nullptr)
            {
                m_rayMarchingInfoBuffer = std::make_unique<ShaderStorageBuffer>("RayMarchingInfoBuffer", sizeOfVector(m_rayMarchingInfos));
            }

            CreateCS(cs, "SSGIDebugGenerateRayCS", SHADER_SOURCE_PATH "ssgi_debug_ray_gen_c.glsl");
            CreateComputePipeline(p, "SSGIDebugGenerateRay", cs);

            // clear data
            for (i32 i = 0; i < SSGISettings.kMaxNumIterationsPerRay; ++i)
            {
                m_rayMarchingInfos[i] = RayMarchingInfo{ };
            }
            m_rayMarchingInfoBuffer->write(m_rayMarchingInfos, 0);

            auto ctx = GfxContext::get();
            p->bind(ctx);
            viewParameters.setShaderParameters(p);
            p->setShaderStorageBuffer(m_debugRayBuffer.get());
            p->setShaderStorageBuffer(m_rayMarchingInfoBuffer.get());

            auto hiZ = render->hiZ();
            p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
            p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
            p->setUniform("settings.kTraceStopMipLevel", (i32)SSGISettings.kTracingStopMipLevel);
            p->setUniform("settings.kMaxNumIterationsPerRay", (i32)SSGISettings.kMaxNumIterationsPerRay);

            p->setTexture("sceneDepthBuffer", render->depth());
            p->setTexture("sceneNormalBuffer", render->normal());
            p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
            p->setUniform("debugRayScreenCoord", m_debugRayScreenCoord);
            glDispatchCompute(1, 1, 1);
            p->unbind(ctx);

            m_debugRayBuffer->read<DebugRay>(m_debugRay, 0);
            m_rayMarchingInfoBuffer->read<RayMarchingInfo>(m_rayMarchingInfos, 0, sizeOfVector(m_rayMarchingInfos));
        }

        {
            auto renderer = Renderer::get();

            // validate hiz buffer first mip
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "HizValidationPS", SHADER_SOURCE_PATH "validate_hiz_p.glsl");
            CreatePixelPipeline(p, "HizValidation", vs, ps);
            RenderTexture2D validationOutput("HizValidation", render->albedo()->getSpec());
            renderer->drawFullscreenQuad(
                getFramebufferSize(render->depth()),
                [render, validationOutput](RenderPass& pass) {
                    pass.setRenderTarget(validationOutput.getGfxTexture2D(), 0);
                },
                p,
                [render](ProgramPipeline* p) {
                    auto hiz = render->hiZ();
                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("depthQuadtree", hiz->m_depthBuffer.get());
                }
            );

            // copy scene rendering output into debug color
            renderer->blitTexture(render->debugColor(), render->resolvedColor());

            // draw screen space ray origin and screen space ray origin with offset
            {
#if 0
                // draw a screen space grid to help visualize a quadtree level
                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "DrawQuadtreeGridPS", SHADER_SOURCE_PATH "draw_quadtree_grid_p.glsl");
                CreatePixelPipeline(p, "DrawQuadtreeGrid", vs, ps);

                RenderTexture2D outColor("DrawQuadtreeGridOutput", render->debugColor()->getSpec());
                renderer->drawFullscreenQuad(
                    getFramebufferSize(render->debugColor()),
                    [outColor](RenderPass& pass) {
                        pass.setRenderTarget(outColor.getGfxTexture2D(), 0, false);
                    },
                    p,
                    [render](ProgramPipeline* p) {
                        auto hiz = render->hiZ();
                        p->setTexture("srcQuadtreeTexture", hiz->m_depthBuffer.get());
                        p->setTexture("backgroundTexture", render->debugColor());
                        p->setUniform("mip", 7);
                    }
                );
                renderer->blitTexture(render->debugColor(), outColor.getGfxTexture2D());
#endif

                std::vector<Renderer::DebugPrimitiveVertex> points(2);
                points[0].position = m_debugRay.screenSpaceRo;
                points[0].color = Color::yellow;
                points[1].position = m_debugRay.screenSpaceRoWithOffset;
                points[1].color = Color::purple;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            // draw ray
            if (m_debugRay.hitResult.x == f32(HitResult::kHit))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = m_debugRay.hitRadiance;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = m_debugRay.hitRadiance;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::green;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            else if (m_debugRay.hitResult.x == f32(HitResult::kBackface))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::purple;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = Color::purple;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::purple;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            else if (m_debugRay.hitResult.x == f32(HitResult::kHidden))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::cyan;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = Color::cyan;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::cyan;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }

            // draw intermediate steps
            {
                std::vector<Renderer::DebugPrimitiveVertex> screenSpacePoints;
                std::vector<Renderer::DebugPrimitiveVertex> screenSpaceDepthSamples;
                std::vector<Renderer::DebugPrimitiveVertex> worldSpacePoints;
                for (i32 iter = 0; iter < SSGISettings.kMaxNumIterationsPerRay; ++iter)
                {
                    const auto& info = m_rayMarchingInfos[iter];
                    if (info.screenSpacePosition.w > 0.f)
                    {
                        Renderer::DebugPrimitiveVertex v0 = { };
                        v0.position = info.screenSpacePosition; 
                        v0.color = Color::blue;
                        screenSpacePoints.push_back(v0);

                        Renderer::DebugPrimitiveVertex v1 = { };
                        v1.position = info.depthSampleCoord; 

                        if (info.screenSpacePosition.z < info.minDepth.x)
                        {
                            v1.color = Color::cyan;
                        }
                        else
                        {
                            v1.color = Color::orange;
                        }
                        screenSpaceDepthSamples.push_back(v1);
                    } 
                    if (info.worldSpacePosition.w > 0.f)
                    {
                        Renderer::DebugPrimitiveVertex v = { };
                        v.position = info.worldSpacePosition;
                        if (info.screenSpacePosition.z < info.minDepth.x)
                        {
                            v.color = Color::pink;
                        }
                        else
                        {
                            v.color = Color::red;
                        }
                        worldSpacePoints.push_back(v);
                    }
                }

                // renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, screenSpacePoints);
                // renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, screenSpaceDepthSamples);
                renderer->debugDrawWorldSpacePoints(render->debugColor(), render->depth(), viewParameters, worldSpacePoints);
            }

            // sample min depth
            {
                const char* src = R"(
#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
struct DepthSample
{
    vec4 sceneDepth;
    vec4 quadtreeDepth;
};
uniform int depthSampleLevel;
uniform vec2 depthSampleCoord;
uniform sampler2D sceneDepthBuffer;
uniform sampler2D depthQuadtree;
layout (std430) buffer DepthSampleBuffer 
{
    DepthSample depthSample;
};
void main() 
{
    float sceneDepth = texture(sceneDepthBuffer, depthSampleCoord).r;
    float quadtreeDepth = textureLod(depthQuadtree, depthSampleCoord, depthSampleLevel).r;
    depthSample.sceneDepth = vec4(vec3(sceneDepth), 1.f);
    depthSample.quadtreeDepth = vec4(vec3(quadtreeDepth), 1.f);
}
                )";
                CreateCSInline(cs, "SSGIDebugMinDepthCS", src);
                CreateComputePipeline(p, "SSGIDebugMinDepth", cs);

                if (m_depthSampleBuffer == nullptr)
                {
                    m_depthSampleBuffer = std::make_unique<ShaderStorageBuffer>("DepthSampleBuffer", sizeof(DepthSample));
                }
                auto ctx = GfxContext::get();
                p->bind(ctx);
                p->setShaderStorageBuffer(m_depthSampleBuffer.get());
                auto hiZ = render->hiZ();
                p->setTexture("depthQuadtree", hiZ->m_depthBuffer.get());
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setUniform("depthSampleCoord", m_depthSampleCoord);
                p->setUniform("depthSampleLevel", m_depthSampleLevel);
                glDispatchCompute(1, 1, 1);
                p->unbind(ctx);
                // read back
                m_depthSampleBuffer->read<DepthSample>(m_depthSample, 0);
                // draw a point on screen to indicate where the depth sample is at
                std::vector<Renderer::DebugPrimitiveVertex> vertices(1);
                vertices[0].position = glm::vec4(m_depthSampleCoord * 2.f - 1.f, 0.f, 1.f);
                vertices[0].color = Color::pink;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, vertices);
            }

            // draw ray start normal
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::blue;
                vertices[1].position = glm::vec4(vec4ToVec3(m_debugRay.ro) + vec4ToVec3(m_debugRay.n), 1.f);
                vertices[1].color = Color::blue;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);
            }
            // draw ray hit normal
        }
    }

    void SSGIDebugger::debugDrawRayOriginAndOffset(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();

        // copy scene rendering output into debug color
        renderer->blitTexture(render->debugColor(), render->resolvedColor());

        // draw screen space ray origin and screen space ray origin with offset
        {
            // draw a screen space grid to help visualize a quadtree level
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "DrawQuadtreeGridPS", SHADER_SOURCE_PATH "draw_quadtree_grid_p.glsl");
            CreatePixelPipeline(p, "DrawQuadtreeGrid", vs, ps);

            RenderTexture2D outColor("DrawQuadtreeGridOutput", render->debugColor()->getSpec());
            renderer->drawFullscreenQuad(
                getFramebufferSize(render->debugColor()),
                [outColor](RenderPass& pass) {
                    pass.setRenderTarget(outColor.getGfxTexture2D(), 0, false);
                },
                p,
                [render](ProgramPipeline* p) {
                    auto hiz = render->hiZ();
                    p->setTexture("srcQuadtreeTexture", hiz->m_depthBuffer.get());
                    p->setTexture("backgroundTexture", render->debugColor());
                    p->setUniform("mip", 5);
                }
            );
            renderer->blitTexture(render->debugColor(), outColor.getGfxTexture2D());

            std::vector<Renderer::DebugPrimitiveVertex> points(2);
            points[0].position = m_debugRay.screenSpaceRo;
            points[0].color = Color::yellow;
            points[1].position = m_debugRay.screenSpaceRoWithOffset;
            points[1].color = Color::purple;
            renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
        }
    }
#endif

    ReSTIRSSGIRenderer::ReSTIRSSGIRenderer()
    {

    }

    ReSTIRSSGIRenderer::~ReSTIRSSGIRenderer()
    {

    }

    // todo: implement this
    struct Reservoir
    {
        Reservoir(const char* reservoirName);

        const char* name = nullptr;
        RenderTexture2D radiance;
        RenderTexture2D position;
        RenderTexture2D normal;
        RenderTexture2D wSumMW;
    };

#if 0
    void ReSTIRSSGIRenderer::renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(ReSTIRSSGIDiffuse, "ReSTIRSSGIDiffuse");

        auto renderer = Renderer::get();

        RenderTexture2D temporalReservoirRadiance("TemporalReservoirRadiance", render->reservoirRadiance());
        RenderTexture2D temporalReservoirPosition("TemporalReservoirPosition", render->reservoirPosition());
        RenderTexture2D temporalReservoirNormal("TemporalReservoirNormal", render->reservoirNormal());
        RenderTexture2D temporalReservoirWSumMW("TemporalReservoirWSumMW", render->reservoirWSumMW());

        // temporal pass
        {
            GPU_DEBUG_SCOPE(ReSTIRSSGITemporal, "ReSTIRSSGITemporal");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "ReSTIRSSGITemporal", SHADER_SOURCE_PATH "ReSTIRSSGI_temporal_p.glsl");
            CreatePixelPipeline(pipeline, "ReSTIRSSGITemporal", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render, temporalReservoirRadiance, temporalReservoirPosition, temporalReservoirNormal, temporalReservoirWSumMW](RenderPass& pass) {
                    {
                        RenderTarget renderTarget(temporalReservoirRadiance.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 0);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirPosition.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 1);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirNormal.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 2);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirWSumMW.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 3);
                    }
                },
                pipeline,
                [this, skyLight, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    auto hiZ = render->hiZ();
                    p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
                    p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
                    p->setUniform("settings.kTraceStopMipLevel", (i32)m_settings.kTracingStopMipLevel);
                    p->setUniform("settings.kMaxNumIterationsPerRay", (i32)m_settings.kMaxNumIterationsPerRay);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("sceneNormalBuffer", render->normal()); 
                    p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
                    p->setTexture("prevFrameIndirectIrradianceBuffer", render->prevFrameIndirectIrradiance());

                    p->setTexture("prevFrameReservoirRadiance", render->prevFrameReservoirRadiance());
                    p->setTexture("prevFrameReservoirPosition", render->prevFrameReservoirPosition());
                    p->setTexture("prevFrameReservoirNormal", render->prevFrameReservoirNormal());
                    p->setTexture("prevFrameReservoirWSumMW", render->prevFrameReservoirWSumMW());
                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());

                    p->setUniform("bTemporalResampling", bTemporalResampling ? 1.f : 0.f);
                }
            );

            // for temporal reprojection related calculations
            renderer->blitTexture(render->prevFrameReservoirRadiance(), temporalReservoirRadiance.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirPosition(), temporalReservoirPosition.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirNormal(), temporalReservoirNormal.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirWSumMW(), temporalReservoirWSumMW.getGfxTexture2D());
        }

        i32 spatialPassSrc, spatialPassDst;
        RenderTexture2D spatialReservoirRadiances[2] = { RenderTexture2D("SpatialReservoirRadiance_0", render->reservoirRadiance()),  RenderTexture2D("SpatialReservoirRadiance_1", render->reservoirRadiance()),};
        RenderTexture2D spatialReservoirPositions[2] = { RenderTexture2D("SpatialReservoirPosition_0", render->reservoirPosition()),  RenderTexture2D("SpatialReservoirPosition_1", render->reservoirPosition()),};
        RenderTexture2D spatialReservoirNormals[2] = { RenderTexture2D("SpatialReservoirNormal_0", render->reservoirNormal()),  RenderTexture2D("SpatialReservoirNormal_1", render->reservoirNormal()),};
        RenderTexture2D spatialReservoirWSumMWs[2] = { RenderTexture2D("SpatialReservoirWSumMW_0", render->reservoirWSumMW()),  RenderTexture2D("SpatialReservoirWSumMW_1", render->reservoirWSumMW()),};

        // spatial pass
        {
            if (bSpatialResampling)
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGISpatial, "ReSTIRSSGISpatial");

                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "ReSTIRSSGISpatial", SHADER_SOURCE_PATH "ReSTIRSSGI_spatial_p.glsl");
                CreatePixelPipeline(pipeline, "ReSTIRSSGISpatial", vs, ps);

                for (i32 pass = 0; pass < m_numSpatialReusePass; ++pass)
                {
                    spatialPassSrc = pass % 2;
                    spatialPassDst = (pass + 1) % 2;

                    if (pass == 0)
                    {
                        renderer->blitTexture(spatialReservoirRadiances[spatialPassSrc].getGfxTexture2D(), temporalReservoirRadiance.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirPositions[spatialPassSrc].getGfxTexture2D(), temporalReservoirPosition.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirNormals[spatialPassSrc].getGfxTexture2D(), temporalReservoirNormal.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirWSumMWs[spatialPassSrc].getGfxTexture2D(), temporalReservoirWSumMW.getGfxTexture2D());
                    }

                    renderer->drawFullscreenQuad(
                        getFramebufferSize(render->indirectIrradiance()),
                        [render, spatialPassSrc, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs](RenderPass& pass) {
                            {
                                RenderTarget renderTarget(spatialReservoirRadiances[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 0);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirPositions[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 1);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirNormals[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 2);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirWSumMWs[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 3);
                            }
                        },
                        pipeline,
                        [this, render, pass, spatialPassSrc, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs, viewParameters](ProgramPipeline* p) {
                            viewParameters.setShaderParameters(p);

                            p->setUniform("reusePass", pass);
                            p->setUniform("reuseSampleCount", m_spatialReuseSampleCount);
                            p->setUniform("reuseRadius", m_spatialReuseRadius);

                            p->setTexture("sceneDepthBuffer", render->depth());
                            p->setTexture("sceneNormalBuffer", render->normal());
                            p->setTexture("inReservoirRadiance", spatialReservoirRadiances[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirPosition", spatialReservoirPositions[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirNormal", spatialReservoirNormals[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirWSumMW", spatialReservoirWSumMWs[spatialPassSrc].getGfxTexture2D());
                            p->setUniform("bUseJacobian", bUseJacobian ? 1.f : 0.f);
                        }
                    );
                }
            }
        }

        // resolve pass
        RenderTexture2D resolved("ReSTIRSSGIResolvedIrradiance", render->indirectIrradiance());
        {
            GPU_DEBUG_SCOPE(ReSTIRSSGIResolve, "ReSTIRSSGIResolve");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "ReSTIRSSGIResolve", SHADER_SOURCE_PATH "ReSTIRSSGI_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "ReSTIRSSGIResolve", vs, ps);

            ShaderSetupFunc withSpatialReuseShaderFunc = [this, render, viewParameters, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs](ProgramPipeline* p) {
                viewParameters.setShaderParameters(p);
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setTexture("sceneNormalBuffer", render->normal()); 
                p->setTexture("reservoirRadiance", spatialReservoirRadiances[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirPosition", spatialReservoirPositions[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirNormal", spatialReservoirNormals[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirWSumMW", spatialReservoirWSumMWs[spatialPassDst].getGfxTexture2D());
            };

            ShaderSetupFunc withoutSpatialReuseShaderFunc = [this, render, viewParameters, temporalReservoirRadiance, temporalReservoirPosition, temporalReservoirNormal, temporalReservoirWSumMW](ProgramPipeline* p) {
                viewParameters.setShaderParameters(p);
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setTexture("sceneNormalBuffer", render->normal()); 
                p->setTexture("reservoirRadiance", temporalReservoirRadiance.getGfxTexture2D());
                p->setTexture("reservoirPosition", temporalReservoirPosition.getGfxTexture2D());
                p->setTexture("reservoirNormal", temporalReservoirNormal.getGfxTexture2D());
                p->setTexture("reservoirWSumMW", temporalReservoirWSumMW.getGfxTexture2D());
            };

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render, resolved](RenderPass& pass) {
                    RenderTarget renderTarget(resolved.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                pipeline,
                bSpatialResampling ? withSpatialReuseShaderFunc : withoutSpatialReuseShaderFunc
            );
        }

        // denoising pass
        {
            if (bDenoisingPass)
            {
                // simple bilateral filtering for now
                GPU_DEBUG_SCOPE(ReSTIRSSGIDenoise, "ReSTIRSSGIDenoise");

                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "ReSTIRSSGIDenoise", SHADER_SOURCE_PATH "ReSTIRSSGI_denoise_p.glsl");
                CreatePixelPipeline(pipeline, "ReSTIRSSGIDenoise", vs, ps);

                i32 src, dst;
                RenderTexture2D denoised[2] = { RenderTexture2D("ReSTIRSSGIResolvedIrradiance_0", render->indirectIrradiance()), RenderTexture2D("ReSTIRSSGIResolvedIrradiance_1", render->indirectIrradiance())};

                for (i32 pass = 0; pass < m_numDenoisingPass; ++pass)
                {
                    src = pass % 2;
                    dst = (pass + 1) % 2;

                    if (pass == 0)
                    {
                        renderer->blitTexture(denoised[src].getGfxTexture2D(), resolved.getGfxTexture2D());
                    }

                    renderer->drawFullscreenQuad(
                        getFramebufferSize(render->indirectIrradiance()),
                        [render, denoised, dst](RenderPass& pass) {
                            RenderTarget renderTarget(denoised[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 0);
                        },
                        pipeline,
                        [this, render, viewParameters, denoised, src](ProgramPipeline* p) {
                            viewParameters.setShaderParameters(p);
                            p->setTexture("sceneDepthBuffer", render->depth());
                            p->setTexture("sceneNormalBuffer", render->normal()); 
                            p->setTexture("indirectIrradiance", denoised[src].getGfxTexture2D());
                        }
                    );
                }

                renderer->blitTexture(render->indirectIrradiance(), denoised[dst].getGfxTexture2D());
            }
            else
            {
                renderer->blitTexture(render->indirectIrradiance(), resolved.getGfxTexture2D());
            }
        }
    }

#define IMGUI_CHECKBOX(name, var)           \
    ImGui::Text(name); ImGui::SameLine();   \
    ImGui::Checkbox("##" name, &var);       \

    void ReSTIRSSGIRenderer::renderUI()
    {
        if (ImGui::TreeNode("ReSTIRSSGI"))
        {
            ImGui::Text("Indirect Boost"); ImGui::SameLine();
            ImGui::SliderFloat("##Indirect Boost", &m_settings.indirectBoost, Settings::kMinIndirectBoost, Settings::kMaxIndirectBoost);

            IMGUI_CHECKBOX("Temporal Resampling", bTemporalResampling)

            IMGUI_CHECKBOX("Spatial Resampling", bSpatialResampling)
            ImGui::Text("Spatial Resampling Radius"); ImGui::SameLine();
            ImGui::SliderFloat("##Spatial Resampling Radius", &m_spatialReuseRadius, 0.f, 1.f);

            IMGUI_CHECKBOX("Denoising", bDenoisingPass)

            ImGui::TreePop();
        }
    }
#endif
}
