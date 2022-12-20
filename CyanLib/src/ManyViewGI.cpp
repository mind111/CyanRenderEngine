#include "ManyViewGI.h"
#include "CyanRenderer.h"
#include "Lights.h"
#include "AssetManager.h"

namespace Cyan {

    glm::mat4 calcHemicubeTangentFrame(const ManyViewGI::Hemicube& hemicube) {
        return calcTangentFrame(hemicube.normal);
    }

    glm::mat4 calcHemicubeTransform(const ManyViewGI::Hemicube& hemicube, const glm::vec3& scale) {
        glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicube);
        glm::mat4 transform = glm::translate(glm::mat4(hemicube.position.w), vec4ToVec3(hemicube.position));
        transform = transform * tangentFrame;
        transform = glm::scale(transform, scale);
        return transform;
    }

    static void drawSurfels(ShaderStorageBuffer<DynamicSsboData<InstanceDesc>>& surfelInstanceBuffer, Renderer* renderer, GfxContext* gfxc, RenderTarget* dstRenderTarget) {
        surfelInstanceBuffer.bind(68);
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        VertexShader* vs = ShaderManager::createShader<VertexShader>("VisualizeSurfelVS", SHADER_SOURCE_PATH "visualize_surfel_v.glsl");
        PixelShader* ps = ShaderManager::createShader<PixelShader>("VisualizeSurfelPS", SHADER_SOURCE_PATH "visualize_surfel_p.glsl");
        PixelPipeline* pipeline = ShaderManager::createPixelPipeline("VisualizeSurfel", vs, ps);
        gfxc->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {

        });
        gfxc->setRenderTarget(dstRenderTarget);
        gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });
        auto va = disk->getSubmesh(0)->getVertexArray();
        gfxc->setVertexArray(va);
        u32 numInstances = surfelInstanceBuffer.getNumElements();
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, numInstances);
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), numInstances);
        }
    }

    ManyViewGI::Image::Image(const glm::uvec2& inIrradianceRes, u32 inFinalGatherRes) 
        : finalGatherRes(inFinalGatherRes)
        , radianceRes(2 * finalGatherRes)
        , irradianceRes(inIrradianceRes)
        , radianceAtlasRes(irradianceRes * radianceRes)
        , hemicubeInstanceBuffer("HemicubeInstanceBuffer")
        , tangentFrameInstanceBuffer("TangentFrameInstanceBuffer") 
    {
        kMaxNumHemicubes = irradianceRes.x * irradianceRes.y;
    }

    void ManyViewGI::Image::initialize() 
    {
        if (!bInitialized) 
        {
            glCreateBuffers(1, &hemicubeBuffer);
            u32 bufferSize = kMaxNumHemicubes * sizeof(Hemicube);
            glNamedBufferData(hemicubeBuffer, bufferSize, nullptr, GL_DYNAMIC_COPY);

            ITextureRenderable::Spec radianceAtlasSpec = { };
            radianceAtlasSpec.type = TEX_2D;
            radianceAtlasSpec.width = radianceAtlasRes.x;
            radianceAtlasSpec.height = radianceAtlasRes.y;
            radianceAtlasSpec.pixelFormat = PF_RGBA16F;
            ITextureRenderable::Parameter params = { };
            params.magnificationFilter = FM_POINT;
            radianceAtlas = new Texture2DRenderable("RadianceAtlas", radianceAtlasSpec, params);

            /** note - @min:
            * it seems imageStore() only supports 1,2,4 channels textures, so using rgba16f here
            */
            {
                ITextureRenderable::Spec irradianceSpec = { };
                irradianceSpec.type = TEX_2D;
                irradianceSpec.width = irradianceRes.x;
                irradianceSpec.height = irradianceRes.y;
                irradianceSpec.pixelFormat = PF_RGBA16F;
                irradiance = new Texture2DRenderable("Irradiance", irradianceSpec);
            }
            {
                ITextureRenderable::Spec spec = { };
                spec.type = TEX_2D;
                spec.width = irradianceRes.x;
                spec.height = irradianceRes.y;
                spec.pixelFormat = PF_RGB16F;
                normal = new Texture2DRenderable("MVGINormal", spec);
                albedo = new Texture2DRenderable("MVGIAlbedo", spec);
            }
            {
                ITextureRenderable::Spec spec = { };
                spec.type = TEX_2D;
                spec.width = irradianceRes.x;
                spec.height = irradianceRes.y;
                spec.pixelFormat = PF_RGBA16F;
                position = new Texture2DRenderable("MVGIPosition", spec);
            }
            {
                ITextureRenderable::Spec spec = { };
                spec.type = TEX_2D;
                spec.width = irradianceRes.x;
                spec.height = irradianceRes.y;
                spec.pixelFormat = PF_RGBA16F;
                directLighting = new Texture2DRenderable("MVGIDirectLighting", spec);
                indirectLighting = new Texture2DRenderable("MVGIIndirectLighting", spec);
                sceneColor = new Texture2DRenderable("MVGISceneColor", spec);
                composed = new Texture2DRenderable("MVGIComposed", spec);
            }

            // register visualizations
            auto renderer = Renderer::get();
            renderer->registerVisualization("ManyViewGI", radianceAtlas);
            renderer->registerVisualization("ManyViewGI", irradiance);
            renderer->registerVisualization("ManyViewGI", position);
            renderer->registerVisualization("ManyViewGI", normal);
            renderer->registerVisualization("ManyViewGI", albedo);
            renderer->registerVisualization("ManyViewGI", directLighting);
            renderer->registerVisualization("ManyViewGI", indirectLighting);
            renderer->registerVisualization("ManyViewGI", sceneColor);
            renderer->registerVisualization("ManyViewGI", composed);

            bInitialized = true;
        }
    }

    /**
    * This pass not only generate hemicubes but also build a "gBuffer" for later use by rendering the scene
    * once and fill out the hemicube ssbo while building other color buffers.
    */
    void ManyViewGI::Image::generateHemicubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        auto renderer = Renderer::get();

        scene->upload();

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(irradianceRes.x, irradianceRes.y));
        renderTarget->setColorBuffer(position, 0);
        renderTarget->setColorBuffer(normal, 1);
        renderTarget->setColorBuffer(albedo, 2);
        renderTarget->setDrawBuffers({ 0, 1, 2 });
        renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 0.f));
        renderTarget->clearDrawBuffer(1, glm::vec4(0.f, 0.f, 0.f, 1.f));
        renderTarget->clearDrawBuffer(2, glm::vec4(0.f, 0.f, 0.f, 1.f));

        // render the scene once to build the @position @normal and @albedo buffer first
        {
            VertexShader* vs = ShaderManager::createShader<VertexShader>("SceneColorPassVS", SHADER_SOURCE_PATH "scene_pass_v.glsl");
            PixelShader* ps = ShaderManager::createShader<PixelShader>("MVGIGBufferPS", SHADER_SOURCE_PATH "mvgi_gbuffer_p.glsl");
            auto pipeline = ShaderManager::createPixelPipeline("MVGIGBuffer", vs, ps);

            auto gfxc = renderer->getGfxCtx();
            gfxc->setDepthControl(DepthControl::kEnable);
            gfxc->setRenderTarget(renderTarget.get());
            gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
            gfxc->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {
            });
            renderer->submitSceneMultiDrawIndirect(*scene);
        }
        // render a second full screen pass for building hemicube buffers
        {
            hemicubes.resize(kMaxNumHemicubes);
            // clear hemicube buffer
            u32 buffSize = sizeof(hemicubes[0]) * hemicubes.size();
            memset(hemicubes.data(), 0x0, buffSize);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 52, hemicubeBuffer);

            VertexShader* vs = ShaderManager::createShader<VertexShader>("BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            PixelShader* ps = ShaderManager::createShader<PixelShader>("GenerateHemicubePS", SHADER_SOURCE_PATH "generate_hemicube_p.glsl");
            auto pipeline = ShaderManager::createPixelPipeline("GenerateHemicube", vs, ps);
            glm::vec2 outputSize(renderTarget->width, renderTarget->height);
            renderer->drawFullscreenQuad(
                renderTarget.get(),
                pipeline,
                [this, outputSize](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("mvgiPosition", position);
                    ps->setTexture("mvgiNormal", normal);
                    ps->setUniform("outputSize", outputSize);
                }
            );
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // read back data from gpu
            glGetNamedBufferSubData(hemicubeBuffer, 0, buffSize, hemicubes.data());

            // update rendering data for generated hemicubes
            hemicubeInstanceBuffer.data.array.clear();
            tangentFrameInstanceBuffer.data.array.clear();
            for (i32 i = 0; i < kMaxNumHemicubes; ++i) {
                if (hemicubes[i].position.w > 0.f) {
                    InstanceDesc desc = { };
                    desc.transform = calcHemicubeTransform(hemicubes[i], glm::vec3(0.1f));
                    desc.atlasTexCoord = glm::ivec2(i % irradiance->width, i / irradiance->width);
                    hemicubeInstanceBuffer.addElement(desc);

                    glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicubes[i]);
                    glm::vec4 colors[3] = {
                        glm::vec4(1.f, 0.f, 0.f, 1.f),
                        glm::vec4(0.f, 1.f, 0.f, 1.f),
                        glm::vec4(0.f, 0.f, 1.f, 1.f)
                    };
                    for (i32 j = 0; j < 3; ++j) {
                        Axis axis = { };
                        axis.v0 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position));
                        axis.v1 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position + tangentFrame[j] * 0.1f));
                        axis.albedo = colors[j];
                        tangentFrameInstanceBuffer.addElement(axis);
                    }
                }
            }
            hemicubeInstanceBuffer.upload();
            tangentFrameInstanceBuffer.upload();
        }
    }

    void ManyViewGI::Image::generateSampleDirections() {
        // generate random directions with in a cone centered around the normal vector
        struct RandomizedCone {
            glm::vec3 sample(const glm::vec3& n) {
                glm::vec2 st = glm::vec2(uniformSampleZeroToOne(), uniformSampleZeroToOne());
                f32 coneRadius = glm::atan(glm::radians(aperture)) * h;
                // a point on disk
                f32 r = sqrt(st.x) * coneRadius;
                f32 theta = st.y * 2.f * M_PI;
                // pp is an uniform random point on the bottom of a cone centered at the normal vector in the tangent frame
                glm::vec3 pp = glm::vec3(glm::vec2(r * cos(theta), r * sin(theta)), 1.f);
                pp = glm::normalize(pp);
                // transform pp back to the world space
                return tangentToWorld(n) * pp;
            }

            const f32 h = 1.f;
            const f32 aperture = 5.f; // in degrees
        } m_sampler;

        jitteredSampleDirections.resize(hemicubes.size());
        for (i32 i = 0; i < kMaxNumHemicubes; ++i) {
            if (hemicubes[i].position.w > 0.f) {
                jitteredSampleDirections[i] = m_sampler.sample(hemicubes[i].normal);
            }
        }
    }

    void ManyViewGI::Image::visualizeHemicubes(RenderTarget* dstRenderTarget) {
        hemicubeInstanceBuffer.bind(63);
        tangentFrameInstanceBuffer.bind(65);

        Renderer* renderer = Renderer::get();
        auto gfxc = renderer->getGfxCtx();
        
        gfxc->setDepthControl(DepthControl::kEnable);
        gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });

        auto visHemicubeVS = ShaderManager::createShader<VertexShader>("VisualizeHemicubeVS", SHADER_SOURCE_PATH "debug_show_hemicubes_v.glsl");
        auto visHemicubePS = ShaderManager::createShader<PixelShader>("VisualizeHemicubePS", SHADER_SOURCE_PATH "debug_show_hemicubes_p.glsl");
        auto visualizeHemicubePipeline = ShaderManager::createPixelPipeline("VisualizeHemicube", visHemicubeVS, visHemicubePS);
        gfxc->setShaderStorageBuffer<DynamicSsboData<InstanceDesc>>(&hemicubeInstanceBuffer);
        gfxc->setPixelPipeline(visualizeHemicubePipeline, [this](VertexShader* vs, PixelShader* ps) {
                ps->setTexture("radianceAtlas", radianceAtlas);
                ps->setUniform("radianceRes", radianceRes);
                ps->setUniform("finalGatherRes", finalGatherRes);
        });

        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        u32 numInstances = hemicubeInstanceBuffer.getNumElements();
        // draw hemicubes
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), numInstances);
        glEnable(GL_CULL_FACE);

        auto drawLineVS = ShaderManager::createShader<VertexShader>("DebugDrawLineVS", SHADER_SOURCE_PATH "debug_draw_line_v.glsl");
        auto drawLinePS = ShaderManager::createShader<PixelShader>("DebugDrawLinePS", SHADER_SOURCE_PATH "debug_draw_line_p.glsl");
        auto drawLinePipeline = ShaderManager::createPixelPipeline("DebugDrawLine", drawLineVS, drawLinePS);
        gfxc->setPixelPipeline(drawLinePipeline, [this](VertexShader* vs, PixelShader* ps) {});
        // draw tangent frames
        glDrawArraysInstanced(GL_LINES, 0, 2, numInstances * 3);
    }

    void ManyViewGI::Image::visualizeIrradiance(RenderTarget* dstRenderTarget) {
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        hemicubeInstanceBuffer.bind(63);

        auto renderer = Renderer::get();
        auto gfxc = renderer->getGfxCtx();
        gfxc->setRenderTarget(dstRenderTarget);
        gfxc->setViewport({ 0, 0, dstRenderTarget->width, dstRenderTarget->height });

        auto visIrradianceVS = ShaderManager::createShader<VertexShader>("VisualizeIrradianceVS", SHADER_SOURCE_PATH "visualize_irradiance_v.glsl");
        auto visIrradiancePS = ShaderManager::createShader<PixelShader>("VisualizeIrradiancePS", SHADER_SOURCE_PATH "visualize_irradiance_p.glsl");
        auto visIrradiancePipeline = ShaderManager::createPixelPipeline("VisualizeIrradiance", visIrradianceVS, visIrradiancePS);
        gfxc->setPixelPipeline(visIrradiancePipeline, [this](VertexShader* vs, PixelShader* ps) {
            ps->setTexture("irradianceBuffer", irradiance);
        });
        auto va = disk->getSubmesh(0)->getVertexArray();
        gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) 
        {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, hemicubeInstanceBuffer.getNumElements());
        }
        else 
        {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), hemicubeInstanceBuffer.getNumElements());
        }
    }


    void ManyViewGI::Image::setup(const RenderableScene& inScene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) 
    {
        // reset workload
        nextHemicube = 0;
        // update scene
        scene.reset(new RenderableScene(inScene));
        // fill hemicubes
        generateHemicubes(depthBuffer, normalBuffer);
        // fill jittered sample directions 
        generateSampleDirections();
    }

    void ManyViewGI::Image::writeRadiance(TextureCubeRenderable* radianceCubemap, const glm::ivec2& texCoord) 
    {
        auto renderer = Renderer::get();
        auto gfxc = renderer->getGfxCtx();
        ComputeShader* cs = ShaderManager::createShader<ComputeShader>("WriteToRadianceAtlasCS", SHADER_SOURCE_PATH "manyview_gi_oct_radiance_c.glsl");
        auto pipeline = ShaderManager::createComputePipeline("WriteToRadianceAtlas", cs);
        gfxc->setComputePipeline(pipeline, [radianceCubemap, texCoord, this](ComputeShader* cs) {
            cs->setTexture("radianceCubemap", radianceCubemap);
            cs->setUniform("texCoord", texCoord);
            cs->setUniform("radianceRes", radianceRes);
        });
        glBindImageTexture(0, radianceAtlas->getGpuObject(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(radianceRes, radianceRes, 1);
    }

    void ManyViewGI::Image::writeIrradiance(TextureCubeRenderable* radianceCubemap, const Hemicube& hemicube, const glm::ivec2& texCoord) 
    {
        auto renderer = Renderer::get();
        auto gfxc = renderer->getGfxCtx();
        CreateCS(cs, "ManyViewGIConvolveIrradianceCS", SHADER_SOURCE_PATH "manyview_gi_convolve_irradiance_c.glsl");
        CreateComputePipeline(pipeline, "ManyViewGIConvolveIrradiance", cs);
        gfxc->setComputePipeline(pipeline, [texCoord, this, radianceCubemap, hemicube](ComputeShader* cs) {
            cs->setUniform("texCoord", texCoord);
            cs->setUniform("finalGatherRes", finalGatherRes);
            cs->setUniform("radianceRes", radianceRes);
            cs->setTexture("radianceCubemap", radianceCubemap);
            cs->setUniform("hemicubeNormal", vec4ToVec3(hemicube.normal));
            auto tangentFrame = calcTangentFrame(hemicube.normal);
            cs->setUniform("hemicubeTangentFrame", tangentFrame);
        });
        glBindImageTexture(0, irradiance->getGpuObject(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(1, 1, 1);
    }

    void ManyViewGI::Image::render(ManyViewGI* gi) 
    {
        if (!finished()) 
        {
            static const u32 perFrameWorkload = 8u;
            for (u32 i = 0; i < perFrameWorkload; ++i)
            {
                const auto& hemicube = hemicubes[nextHemicube];
                if (hemicube.position.w > 0.f) 
                {
                    auto radianceCubemap = gi->finalGathering(hemicubes[nextHemicube], *scene, true, jitteredSampleDirections[nextHemicube]);
                    u32 x = nextHemicube % irradianceRes.x;
                    u32 y = nextHemicube / irradianceRes.x;
                    writeRadiance(radianceCubemap, glm::ivec2(x, y));
                    writeIrradiance(radianceCubemap, hemicube, glm::ivec2(x, y));
                }
                nextHemicube++;
            }
        }
        else
        {
            renderDirectLighting();
            renderIndirectLighting();
            compose();
        }
    }

    void ManyViewGI::Image::renderDirectLighting()
    {
        if (directLighting)
        {
            VertexShader* vs = ShaderManager::createShader<VertexShader>("BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            PixelShader* ps = ShaderManager::createShader<PixelShader>("MVGIDirectLighting", SHADER_SOURCE_PATH "mvgi_direct_lighting.glsl");
            auto pipeline = ShaderManager::createPixelPipeline("mvgiDirectLighting", vs, ps);
            auto renderer = Renderer::get();
            auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(directLighting->width, directLighting->height));
            renderTarget->setColorBuffer(directLighting, 0);
            renderer->drawFullscreenQuad(
                renderTarget.get(),
                pipeline,
                [this](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("mvgiPosition", position);
                    ps->setTexture("mvgiNormal", normal);
                    ps->setTexture("mvgiAlbedo", albedo);
                }
            );
        }
    }

    void ManyViewGI::Image::renderIndirectLighting()
    {
        if (indirectLighting)
        {
            VertexShader* vs = ShaderManager::createShader<VertexShader>("BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            PixelShader* ps = ShaderManager::createShader<PixelShader>("MVGIIndirectLighting", SHADER_SOURCE_PATH "mvgi_indirect_lighting.glsl");
            auto pipeline = ShaderManager::createPixelPipeline("mvgiIndirectLighting", vs, ps);
            auto renderer = Renderer::get();
            auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(indirectLighting->width, indirectLighting->height));
            renderTarget->setColorBuffer(indirectLighting, 0);
            renderer->drawFullscreenQuad(
                renderTarget.get(),
                pipeline,
                [this](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("irradiance", irradiance);
                    ps->setTexture("mvgiAlbedo", albedo);
                }
            );
        }
    }

    void ManyViewGI::Image::compose()
    {
        auto renderer = Renderer::get();
        if (directLighting && irradiance)
        {
            auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(sceneColor->width, sceneColor->height));
            renderTarget->setColorBuffer(sceneColor, 0);
            VertexShader* vs = ShaderManager::createShader<VertexShader>("BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            PixelShader* ps = ShaderManager::createShader<PixelShader>("MVGIComposeLighting", SHADER_SOURCE_PATH "mvgi_compose_lighting.glsl");
            auto pipeline = ShaderManager::createPixelPipeline("MVGIComposeLighting", vs, ps);
            // compose lighting
            renderer->drawFullscreenQuad(
                renderTarget.get(),
                pipeline,
                [this](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("mvgiDirectLighting", directLighting);
                    ps->setTexture("mvgiIndirectLighting", indirectLighting);
                }
            );
            // post
            // auto bloom = renderer->bloom(sceneColor);
            renderer->compose(composed, sceneColor, nullptr, glm::uvec2(composed->width, composed->height));
        }
    }

    ManyViewGI::ManyViewGI(Renderer* renderer, GfxContext* gfxc) 
        : m_renderer(renderer), m_gfxc(gfxc) 
    {
    }

    void ManyViewGI::initialize() 
    {
        if (!bInitialized) {
            if (!m_sharedRadianceCubemap) {
                ITextureRenderable::Spec spec = {};
                spec.width = kFinalGatherRes;
                spec.height = kFinalGatherRes;
                spec.type = TEX_CUBE;
                spec.pixelFormat = PF_RGB16F;
                ITextureRenderable::Parameter params = {};
                /** note - @mind:
                * ran into a "incomplete texture" error when setting magnification filter to FM_TRILINEAR
                */
                // todo: based on the rendering output, pay attention to the difference between using point/bilinear sampling
                params.minificationFilter = FM_POINT;
                params.magnificationFilter = FM_POINT;
                params.wrap_r = WM_CLAMP;
                params.wrap_s = WM_CLAMP;
                params.wrap_t = WM_CLAMP;
                m_sharedRadianceCubemap = new TextureCubeRenderable("ManyViewGISharedRadianceCubemap", spec);
            }
            if (!visualizations.shared) {
                ITextureRenderable::Spec spec = {};
                spec.type = TEX_2D;
                spec.width = visualizations.resolution.x;
                spec.height = visualizations.resolution.y;
                spec.pixelFormat = PF_RGB16F;
                visualizations.shared = new Texture2DRenderable("ManyViewGIVisualization", spec);
            }
            m_renderer->registerVisualization("ManyViewGI", visualizations.shared);

            customInitialize();
            bInitialized = true;
        }
    }

    void ManyViewGI::customInitialize() { 
        if (!m_image) {
            m_image = new Image(kImageResolution, kFinalGatherRes);
            m_image->initialize();
        }
    }

    void ManyViewGI::setup(const RenderableScene& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) 
    {
        m_image->setup(scene, depthBuffer, normalBuffer);
        customSetup(*m_scene, depthBuffer, normalBuffer);
    }

    void ManyViewGI::render(RenderTarget* renderTarget, const RenderableScene& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        m_image->render(this);

        auto visRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(
            visualizations.shared->width, 
            visualizations.shared->height)
        );
        visRenderTarget->setColorBuffer(visualizations.shared, 0);
        visRenderTarget->setDrawBuffers({ 0 });
        visRenderTarget->clearDrawBuffer(0, glm::vec4(.2f, .2f, .2f, 1.f));
        if (opts.bVisualizeIrradiance) {
            m_image->visualizeIrradiance(visRenderTarget.get());
        }
        if (opts.bVisualizeHemicubes) {
            m_image->visualizeHemicubes(visRenderTarget.get());
        }

        customRender(scene.camera, renderTarget, visRenderTarget.get());

        // ui controls
        m_renderer->addUIRenderCommand([this, scene, depthBuffer, normalBuffer]() {
            ImGui::Begin("ManyView GI"); {
                if (ImGui::Button("Build Indirect Lighting")) {
                    setup(scene, depthBuffer, normalBuffer);
                }
                ImGui::Checkbox("Visualize Hemicubes", &opts.bVisualizeHemicubes);
                ImGui::Checkbox("Visualize Irradiance", &opts.bVisualizeIrradiance);

                customUI();
            } ImGui::End();
        });
    }

    TextureCubeRenderable* ManyViewGI::finalGathering(const Hemicube& hemicube, RenderableScene& scene, bool jitter, const glm::vec3& jitteredSampleDirection) 
    {
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(m_sharedRadianceCubemap->resolution, m_sharedRadianceCubemap->resolution));
        renderTarget->setColorBuffer(m_sharedRadianceCubemap, 0);
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        // calculate the tangent frame of radiance cube
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = hemicube.normal;
        if (jitter) 
        {
            up = jitteredSampleDirection;
        }
        if (abs(up.y) > 0.98) 
        {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        glm::vec3 right = glm::cross(worldUp, up);
        glm::vec3 forward = glm::cross(up, right);
        glm::mat3 tangentFrame = {
            right,
            up,
            -forward
        };
        for (i32 face = 0; face < 6; ++face) 
        {
            renderTarget->setDrawBuffers({ face });
            renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f));

            // skip the bottom face as it doesn't contribute to shared at all
            if (face == 3) {
                continue;
            }

            /** note - @min:
            * the order of camera facing directions should match the following
                    {1.f, 0.f, 0.f},   // Right
                    {-1.f, 0.f, 0.f},  // Left
                    {0.f, 1.f, 0.f},   // Up
                    {0.f, -1.f, 0.f},  // Down
                    {0.f, 0.f, 1.f},   // Front
                    {0.f, 0.f, -1.f},  // Back
            */
            glm::vec3 cameraFacingDirs[] = {
                right,
                -right,
                up,
                -up,
                -forward,
                forward
            };
            glm::vec3 lookAtDir = tangentFrame * LightProbeCameras::cameraFacingDirections[face];
            PerspectiveCamera camera(
                glm::vec3(hemicube.position),
                glm::vec3(hemicube.position) + lookAtDir,
                tangentFrame * LightProbeCameras::worldUps[face],
                90.f,
                0.01f,
                32.f,
                1.0f
            );
            customRenderScene(scene, hemicube, camera);
            if (scene.skybox) 
            {
                u32 a = scene.skybox->m_cubemapTexture->numMips - 1;
                u32 b = log2f(kFinalGatherRes);
                f32 mip = f32(a - b);
                scene.skybox->render(renderTarget.get(), camera.view(), camera.projection(), mip);
            }
        }
        return m_sharedRadianceCubemap;
    }

    void ManyViewGI::customRenderScene(RenderableScene& scene, const Hemicube& hemicube, const PerspectiveCamera& camera) 
    {
        scene.camera = RenderableScene::Camera(camera);
        scene.upload();
        CreateVS(vs, "ManyViewGIVS", SHADER_SOURCE_PATH "manyview_gi_final_gather_v.glsl");
        CreatePS(ps, "ManyViewGIPS", SHADER_SOURCE_PATH "manyview_gi_final_gather_p.glsl");
        CreatePixelPipeline(pipeline, "ManyViewGI", vs, ps);
        m_gfxc->setPixelPipeline(pipeline, [camera, hemicube, this](VertexShader* vs, PixelShader* ps) {
        });
        m_renderer->submitSceneMultiDrawIndirect(scene);
    }

    PointBasedManyViewGI::PointBasedManyViewGI(Renderer* renderer, GfxContext* gfxc)
        : ManyViewGI(renderer, gfxc) 
        , surfelBuffer("SurfelBuffer")
        , surfelInstanceBuffer("surfelInstanceBuffer")
    {

    }

    void PointBasedManyViewGI::customInitialize() 
    { 
        if (!bInitialized) 
        {
            if (!visualizations.rasterizedSurfels) 
            {
                ITextureRenderable::Spec spec = {};
                spec.type = TEX_2D;
                spec.width = 16;
                spec.height = 16;
                spec.pixelFormat = PF_RGB16F;
                visualizations.rasterizedSurfels = new Texture2DRenderable("RasterizedSurfelScene", spec);
            }
            bInitialized = true;
        }
    }

    void PointBasedManyViewGI::customSetup(const RenderableScene& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) 
    {
        generateWorldSpaceSurfels();
        cacheSurfelDirectLighting();
    }

    void PointBasedManyViewGI::customRender(const RenderableScene::Camera& camera, RenderTarget* renderTarget, RenderTarget* visRenderTarget) {
        if (m_scene) {
            if (bVisualizeSurfels) {
                visualizeSurfels(visRenderTarget);
                rasterizeSurfelScene(visualizations.rasterizedSurfels, m_scene->camera);
            }
        }
    }

    void PointBasedManyViewGI::customUI() {
        ImGui::Checkbox("Visualize Surfels", &bVisualizeSurfels);
    }

    // todo: implement this on gpu using geometry shader
    void PointBasedManyViewGI::generateWorldSpaceSurfels() {
        surfels.clear();
        surfelBuffer.data.array.clear();
        // calculate total surface area by summing area of all triangles in the scene
        for (i32 i = 0; i < m_scene->meshInstances.size(); ++i) {
            auto meshInst = m_scene->meshInstances[i];
            auto mesh = meshInst->parent;
            glm::mat4 transform = (*m_scene->transformBuffer)[i];
            glm::mat4 normalTransform = glm::inverse(glm::transpose(transform));
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                auto submesh = dynamic_cast<Mesh::Submesh<Triangles>*>(mesh->getSubmesh(sm));
                if (submesh) {
                    glm::vec3 albedo(0.f);
                    auto matl = meshInst->getMaterial(sm);
                    // todo: figure out how to handle textured material
                    if (matl) {
                        albedo = matl->albedo;
                    }
                    const auto& verts = submesh->getVertices();
                    const auto& indices = submesh->getIndices();
                    i32 numTriangles = submesh->numIndices() / 3;
                    for (i32 t = 0; t < numTriangles; ++t) {
                        const auto& v0 = verts[indices[(u64)t * 3 + 0]];
                        const auto& v1 = verts[indices[(u64)t * 3 + 1]];
                        const auto& v2 = verts[indices[(u64)t * 3 + 2]];

                        // transform vertices
                        glm::vec3 p0 = transform * glm::vec4(v0.pos, 1.f);
                        glm::vec3 p1 = transform * glm::vec4(v1.pos, 1.f);
                        glm::vec3 p2 = transform * glm::vec4(v2.pos, 1.f);

                        // transform normal
                        glm::vec3 n0 = glm::normalize(normalTransform * glm::vec4(v0.normal, 0.f));
                        glm::vec3 n1 = glm::normalize(normalTransform * glm::vec4(v1.normal, 0.f));
                        glm::vec3 n2 = glm::normalize(normalTransform * glm::vec4(v2.normal, 0.f));

                        // calculate triangle area
                        f32 area = .5f * glm::length(glm::cross(p1 - p0, p2 - p0));
                        // todo: this doesn't work for densely tessellated meshes such as the stanford bunny and dragon meshes
                        i32 numSurfels = (area * 2.f) / (3.1415926f * kSurfelRadius * kSurfelRadius);
                        for (i32 s = 0; s < numSurfels; ++s) {
                            glm::vec2 st = halton23(s);
                            // rejection sampling
                            i32 numRejected = 0;
                            while (st.x + st.y > 1.f) {
                                st = halton23(s + numRejected);
                                numRejected++;
                            }
                            f32 w = 1.f - st.x - st.y;
                            surfels.emplace_back(Surfel{
                                st.x * p0 + st.y * p1 + w * p2,
                                glm::normalize(st.x * n0 + st.y * n1 + w * n2),
                                albedo,
                                kSurfelRadius,
                            });
                            surfelBuffer.addElement(GpuSurfel{
                                glm::vec4(st.x * p0 + st.y * p1 + w * p2, 1.f),
                                glm::vec4(glm::normalize(st.x * n0 + st.y * n1 + w * n2), 0.f),
                                glm::vec4(albedo, 1.f),
                                glm::vec4(0.f, 0.f, 0.f, 1.f)
                            });
                        }
                    }
                }
            }
        }
        surfelBuffer.upload();
        updateSurfelInstanceData();
    }

    void PointBasedManyViewGI::cacheSurfelDirectLighting() {
#if 0
        if (m_scene) {
            auto shader = ShaderManager::createShader({
                ShaderSource::Type::kCs,
                "RenderSurfelDirectLightingShader",
                nullptr,
                nullptr,
                nullptr,
                SHADER_SOURCE_PATH "surfel_direct_lighting_c.glsl"
            });

            surfelBuffer.bind(69);

            m_gfxc->setShader(shader);
            // each compute thread process one surfel
            glDispatchCompute(surfels.size(), 1, 1);
        }
#endif
    }

    void PointBasedManyViewGI::rasterizeSurfelScene(Texture2DRenderable* outSceneColor, const RenderableScene::Camera& camera) {
#if 0
        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(outSceneColor->width, outSceneColor->height));
        renderTarget->setColorBuffer(outSceneColor, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer(0, glm::vec4(0.2f, .2f, .2f, 1.f));

        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "RasterizeSurfelShader",
            SHADER_SOURCE_PATH "rasterize_surfels_v.glsl",
            SHADER_SOURCE_PATH "rasterize_surfels_p.glsl"
        });

        assert(surfels.size() == surfelBuffer.getNumElements());

        surfelBuffer.bind(69);

        m_gfxc->setShader(shader);
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        shader->setUniform("outputSize", glm::ivec2(renderTarget->width, renderTarget->height));
        shader->setUniform("cameraFov", camera.fov);
        shader->setUniform("cameraN", camera.n);
        shader->setUniform("view", camera.view);
        shader->setUniform("projection", camera.projection);
        shader->setUniform("receivingNormal", m_scene->camera.view[1]);
        glDrawArraysInstanced(GL_POINTS, 0, 1, surfels.size());
#endif
    }

    void PointBasedManyViewGI::customRenderScene(RenderableScene& scene, const Hemicube& hemicube, const PerspectiveCamera& camera) 
    {
#if 0
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "RasterizeSurfelShader",
            SHADER_SOURCE_PATH "rasterize_surfels_v.glsl",
            SHADER_SOURCE_PATH "rasterize_surfels_p.glsl"
        });
        assert(surfels.size() == surfelBuffer.getNumElements());

        surfelBuffer.bind(69);

        shader->setUniform("outputSize", glm::ivec2(kFinalGatherRes, kFinalGatherRes));
        shader->setUniform("cameraFov", camera.fov);
        shader->setUniform("cameraN", camera.n);
        shader->setUniform("surfelRadius", kSurfelRadius);
        shader->setUniform("view", camera.view());
        shader->setUniform("projection", camera.projection());
        shader->setUniform("hemicubeNormal", vec4ToVec3(hemicube.normal));
        m_gfxc->setShader(shader);
        glDrawArraysInstanced(GL_POINTS, 0, 1, surfels.size());
#endif
    }

    void PointBasedManyViewGI::updateSurfelInstanceData() 
    {
#if 0
        surfelInstanceBuffer.data.array.resize(surfels.size());
        for (i32 i = 0; i < surfels.size(); ++i) 
        {
            glm::mat4 transform = glm::translate(glm::mat4(1.f), surfels[i].position);
            transform = transform * calcTangentFrame(surfels[i].normal);
            surfelInstanceBuffer[i].transform = glm::scale(transform, glm::vec3(surfels[i].radius));
            surfelInstanceBuffer[i].albedo = glm::vec4(surfels[i].albedo, 1.f);
            surfelInstanceBuffer[i].radiance = glm::vec4(0.f, 0.f, 0.f, 1.f);
            surfelInstanceBuffer[i].debugColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
        }
        surfelInstanceBuffer.upload();
#endif
    }

    void PointBasedManyViewGI::visualizeSurfels(RenderTarget* visRenderTarget) {
#if 0
        surfelInstanceBuffer.bind(68);
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeSurfelShader",
            SHADER_SOURCE_PATH "visualize_surfel_v.glsl",
            SHADER_SOURCE_PATH "visualize_surfel_p.glsl"
            });
        m_gfxc->setRenderTarget(visRenderTarget);
        m_gfxc->setViewport({ 0, 0, visRenderTarget->width, visRenderTarget->height });
        m_gfxc->setShader(shader);
        shader->setUniform("visMode", (u32)(m_visMode));
        auto va = disk->getSubmesh(0)->getVertexArray();
        m_gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, surfelInstanceBuffer.getNumElements());
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), surfelInstanceBuffer.getNumElements());
        }
#endif
    }

    MicroRenderingGI::MicroRenderingGI(Renderer* renderer, GfxContext* gfxc)
        : PointBasedManyViewGI(renderer, gfxc), m_softwareMicroBuffer(24) {

    }

    void MicroRenderingGI::customInitialize() {
        PointBasedManyViewGI::customInitialize();

        if (!bInitialized) {
            bInitialized = true;
            m_renderer->registerVisualization("ManyViewGI", m_surfelBSH.getVisualization(), &bVisualizeSurfelBSH);
            m_renderer->registerVisualization("ManyViewGI", m_softwareMicroBuffer.getVisualization(), &bVisualizeMicroBuffer);
        }
    }

    void MicroRenderingGI::customSetup(const RenderableScene& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        PointBasedManyViewGI::customSetup(scene, depthBuffer, normalBuffer);
    }

    void MicroRenderingGI::generateWorldSpaceSurfels() 
    {
        m_surfelSampler.sampleFixedNumberSurfels(surfels, *m_scene);
        m_surfelSampler.sampleFixedSizeSurfels(surfels, *m_scene);
        for (const auto& surfel : surfels) 
        {
            surfelBuffer.addElement(
                GpuSurfel {
                    glm::vec4(surfel.position, surfel.radius),
                    glm::vec4(surfel.normal, 0.f),
                    glm::vec4(surfel.albedo, 1.f),
                    glm::vec4(0.f, 0.f, 0.f, 1.f)
                }
            );
        }
        surfelBuffer.upload();
        m_surfelBSH.build(surfels);
    }

    void MicroRenderingGI::customRender(const RenderableScene::Camera& camera, RenderTarget* renderTarget, RenderTarget* visRenderTarget) 
    {
        PointBasedManyViewGI::customRender(camera, renderTarget, visRenderTarget);
        if (bVisualizeSurfelBSH) 
        {
            m_surfelBSH.visualize(m_gfxc, visRenderTarget);
        }
        if (bVisualizeSurfelSampler) 
        {
            m_surfelSampler.visualize(renderTarget, m_renderer);
        }
        if (bVisualizeMicroBuffer) 
        {
            // todo: verify correctness
            softwareMicroRendering(camera, m_surfelBSH);
            // todo: implement this once the software version is working
            // hardwareMicroRendering(m_scene->camera, m_surfelBSH);
            m_softwareMicroBuffer.visualize(m_renderer, visRenderTarget);
        }
    }

    SoftwareMicroBuffer::SoftwareMicroBuffer(i32 microBufferRes) 
     : resolution(microBufferRes)
        , color(resolution)
        , depth(resolution)
        , indices(resolution)
        , postTraversalBuffer(resolution) 
    {
        {
            ITextureRenderable::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = resolution;
            spec.height = resolution;
            spec.pixelFormat = PF_RGB32F;
            ITextureRenderable::Parameter params = { };
            params.magnificationFilter = FM_POINT;
            gpuTexture = new Texture2DRenderable("MicroBuffer", spec, params);
        }

        {
            ITextureRenderable::Spec spec = { };
            spec.type = TEX_2D;
            spec.width = 1280;
            spec.height = 720;
            spec.pixelFormat = PF_RGB16F;
            visualization = new Texture2DRenderable("MicroBufferVis", spec);
        }

        clear();
    }

    void SoftwareMicroBuffer::clear() {
        for (i32 y = 0; y < resolution; ++y) {
            for (i32 x = 0; x < resolution; ++x) {
                color[y][x] = glm::vec3(0.1f);
                depth[y][x] = FLT_MAX;
                indices[y][x] = -1;
                postTraversalBuffer[y][x] = -1;
            }
        }
        // glClearTexSubImage(gpuTexture->getGpuObject(), 0, 0, 0, 0, resolution, resolution, 1, GL_RGB, GL_FLOAT, nullptr);
    }

    bool SoftwareMicroBuffer::isInViewport(const SurfelBSH::Node& node, glm::vec2& outPixelCoord, const glm::mat4& view, const glm::mat4& projection) {
        glm::vec4 pos = projection * view * glm::vec4(node.center, 1.f);
        pos /= pos.w;
        outPixelCoord.x = pos.x * .5f + .5f;
        outPixelCoord.y = pos.y * .5f + .5f;
        return (outPixelCoord.x >= 0.f && outPixelCoord.x <= 1.f && outPixelCoord.y >= 0.f && outPixelCoord.y <= 1.f);
    }

    f32 SoftwareMicroBuffer::solidAngleOfSphere(const glm::vec3& p, const glm::vec3& q, f32 r) {
        f32 d = glm::length(p - q);
        if (d >= r) {
            return 2 * M_PI * (1.f - sqrt(d * d - r * r) / d);
        }
        else {
            return -1.f;
        }
    }

    /** note - @min: 
    * reference: https://www.rorydriscoll.com/2012/01/15/cubemap-texel-solid-angle/
    */
    f32 SoftwareMicroBuffer::calcCubemapTexelSolidAngle(const glm::vec3& d, f32 cubemapRes) {
        auto solidAngleHelper = [](const glm::vec2& st) {
            return glm::atan(st.x * st.y, sqrt(st.x * st.x + st.y * st.y + 1.f));
        };

        f32 solidAngle;
        glm::vec3 dd = abs(d);
        glm::vec2 texCoord;
        if (dd.x > dd.y && dd.x > dd.z) {
            if (d.x > 0.f) {
                texCoord = glm::vec2(-d.z, d.y) / dd.x;
            } else {
                texCoord = glm::vec2(d.z, d.y) / dd.x;
            }
        }
        if (dd.y > dd.x && dd.y > dd.z) {
            if (d.y > 0.f) {
                texCoord = glm::vec2(-d.z, -d.x) / dd.y;
            } else {
                texCoord = glm::vec2(-d.z, d.x) / dd.y;
            }
        }
        else if (dd.z > dd.y && dd.z > dd.x) {
            if (d.z > 0.f) {
                texCoord = glm::vec2(d.x, d.y) / dd.z;
            } else {
                texCoord = glm::vec2(-d.x, d.y) / dd.z;
            }
        }
        float texelSize = 2.f / cubemapRes;
        // find 4 corners of the pixel
        glm::vec2 A = floor(texCoord * cubemapRes * .5f) / (cubemapRes * .5f);
        glm::vec2 B = A + glm::vec2(0.f, 1.f) * texelSize;
        glm::vec2 C = A + glm::vec2(1.f, 0.f) * texelSize;
        glm::vec2 D = A + glm::vec2(1.f, 1.f) * texelSize;
        solidAngle = solidAngleHelper(A) + solidAngleHelper(D) - solidAngleHelper(B) - solidAngleHelper(C);
        return solidAngle;
    }

    void SoftwareMicroBuffer::traverseBSH(const SurfelBSH& surfelBSH, i32 nodeIndex, const RenderableScene::Camera& camera) {
        if (nodeIndex < 0) {
            return;
        }

        glm::vec3 lightDir = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
        const SurfelBSH::Node& node = surfelBSH.nodes[nodeIndex];
        glm::mat4 view = camera.view;
        glm::mat4 projection = camera.projection;
        glm::vec3 cameraPos = camera.eye;
        glm::vec3 v = node.center - cameraPos;
        f32 dist = length(v);
        glm::vec2 pixelCoord;
        
        if (isInViewport(node, pixelCoord, view, projection)) {
            i32 x = floor(pixelCoord.x * resolution);
            i32 y = floor(pixelCoord.y * resolution);
            // todo: try use other metrics to determine whether higher LOD is necessary such as projected size in screen space
            f32 nodeSolidAngle = solidAngleOfSphere(cameraPos, node.center, node.radius);
            f32 pixelSolidAngle = calcCubemapTexelSolidAngle(normalize(v), resolution);
            /** note:- @min:
            * if the projected screen space position of a node's center is too far away from the pixel center
            * then it's likely that this node's footprint touches few neighbor pixels as well. need to address
            * this kind of pixels either by
            *   * adding them into post traversal list
            *   * write it to neighbor pixels that are within a bilinear tap and perform depth test
            */
            bool stopRecursion = (nodeSolidAngle >= 0.f && nodeSolidAngle < pixelSolidAngle);
            if (stopRecursion) {
                if (dist <= depth[y][x]) {
                    depth[y][x] = dist;
                    f32 ndotl = glm::max(glm::dot(glm::normalize(glm::vec3(1.f, 1.f, 1.f)), node.normal), 0.f) * .5f + .5f;
                    color[y][x] = node.albedo * ndotl;
#if 0
                    if (isLeafNode(node)) {
                        // todo: weird color bug, gb seems to be flipped
                        microBuffer.color[y][x] = glm::vec3(1.f, 0.f, 0.f);
                    }
#endif
                }
                return;
            }
            else {
                if (node.isLeaf()) {
                    if (dist <= depth[y][x]) {
                        depth[y][x] = dist;
                        f32 ndotl = glm::max(glm::dot(glm::normalize(glm::vec3(1.f, 1.f, 1.f)), node.normal), 0.f) * .5f + .5f;
                        color[y][x] = node.albedo * ndotl;
                        postTraversalBuffer[y][x] = nodeIndex;
                    }
                }
            }
        }
        traverseBSH(surfelBSH, node.childs[0], camera);
        traverseBSH(surfelBSH, node.childs[1], camera);
    }

    void SoftwareMicroBuffer::raytraceInternal(const glm::vec3& ro, const glm::vec3& rd, const SurfelBSH& surfelBSH, i32 nodeIndex, f32& tmin, i32& hitNode) {
        const SurfelBSH::Node node = surfelBSH.nodes[nodeIndex];
#if 1
        // stop recursion at proper LOD
        f32 t;
        if (node.intersect(ro, rd, t)) {
            if (node.isLeaf()) {
                if (t <= tmin) {
                    tmin = t;
                    hitNode = nodeIndex;
                }
                return;
            }
            f32 nodeSolidAngle = solidAngleOfSphere(ro, node.center, node.radius);
            f32 pixelSolidAngle = calcCubemapTexelSolidAngle(rd, resolution);
            bool bKeepRecursion = (nodeSolidAngle < 0.f || nodeSolidAngle >= pixelSolidAngle);
            if (bKeepRecursion) {
                raytraceInternal(ro, rd, surfelBSH, node.childs[0], tmin, hitNode);
                raytraceInternal(ro, rd, surfelBSH, node.childs[1], tmin, hitNode);
            }
            else {
                tmin = t;
                hitNode = nodeIndex;
                return;
            }
        }
#else
        // brute-force ray tracing
        f32 t;
        if (node.intersect(ro, rd, t)) {
            if (node.isLeaf()) {
                if (t <= tmin) {
                    hitNode = nodeIndex;
                    tmin = t;
                }
            }
            else {
                raytraceInternal(ro, rd, surfelBSH, node.childs[0], tmin, hitNode);
                raytraceInternal(ro, rd, surfelBSH, node.childs[1], tmin, hitNode);
            }
        }
#endif
    }

    void SoftwareMicroBuffer::raytrace(const RenderableScene::Camera& inCamera, const SurfelBSH& surfelBSH) {
        clear();

        RenderableScene::Camera camera = { };
        camera.eye = inCamera.eye;
        camera.lookAt = inCamera.lookAt;
        camera.right = inCamera.right;
        camera.forward = inCamera.forward;
        camera.up = inCamera.up;
        camera.fov = glm::radians(90.f);
        camera.aspect = 1.f;
        camera.n = inCamera.n;
        camera.f = inCamera.f;
        camera.view = glm::lookAt(camera.eye, camera.lookAt, glm::vec3(0.f, 1.f, 0.f));
        camera.projection = glm::perspective(camera.fov, camera.aspect, camera.n, camera.f);

        f32 imagePlaneSize = camera.n;
        for (i32 y = 0; y < resolution; ++y) {
            for (i32 x = 0; x < resolution; ++x) {
                glm::vec2 pixelCoord = glm::vec2(((f32)x + .5f) / resolution, ((f32)y + .5f) / resolution);
                pixelCoord = pixelCoord * 2.f - 1.f;
                glm::vec3 ro = camera.eye;
                glm::vec3 rd = glm::normalize(camera.right * pixelCoord.x * imagePlaneSize + camera.up * pixelCoord.y * imagePlaneSize + camera.n * camera.forward);
                f32 tmin = FLT_MAX;
                i32 hitNode = -1;
                raytraceInternal(ro, rd, surfelBSH, 0, tmin, hitNode);
                if (hitNode >= 0) {
                    color[y][x] = surfelBSH.nodes[hitNode].albedo;
                }
            }
        }

        // update texture data using micro buffer
        if (gpuTexture) {
            glTextureSubImage2D(gpuTexture->getGpuObject(), 0, 0, 0, resolution, resolution, GL_RGB, GL_FLOAT, color.data());
        }
    }

    void SoftwareMicroBuffer::postTraversal(const RenderableScene::Camera& camera, const SurfelBSH& surfelBSH) {
        postTraversalList.clear();
        for (i32 y = 0; y < resolution; ++y) {
            for (i32 x = 0; x < resolution; ++x) {
                i32 nodeIndex = postTraversalBuffer[y][x];
                if (nodeIndex >= 0) {
                    postTraversalList.push_back(surfelBSH.nodes[nodeIndex]);
                }
            }
        }

        f32 imagePlaneSize = camera.n;
        for (i32 y = 0; y < resolution; ++y) {
            for (i32 x = 0; x < resolution; ++x) {
                glm::vec2 pixelCoord = glm::vec2(((f32)x + .5f) / resolution, ((f32)y + .5f) / resolution);
                pixelCoord = pixelCoord * 2.f - 1.f;
                glm::vec3 ro = camera.eye;
                glm::vec3 rd = glm::normalize(camera.right * pixelCoord.x * imagePlaneSize + camera.up * pixelCoord.y * imagePlaneSize + camera.n * camera.forward);

                f32 tmin = FLT_MAX;
                i32 hitNode = -1;
                for (i32 i = 0; i < postTraversalList.size(); ++i) {
                    f32 t;
                    if (postTraversalList[i].intersect(ro, rd, t)) {
                        if (t <= tmin) {
                            tmin = t;
                            hitNode = i;
                        }
                    }
                }
                if (hitNode >= 0) {
                    f32 ndotl = glm::max(glm::dot(glm::normalize(glm::vec3(0.f, 1.f, 0.f)), postTraversalList[hitNode].normal), 0.f) * .5f + .5f;
                    color[y][x] = postTraversalList[hitNode].albedo * ndotl;
                    depth[y][x] = glm::length(postTraversalList[hitNode].center - camera.eye);
                }
            }
        }
    }

    // todo: implement and verify this
    // todo: view frustum culling
    // todo: cache lighting at each node
    void SoftwareMicroBuffer::render(const RenderableScene::Camera& inCamera, const SurfelBSH& surfelBSH) {
        clear();

        RenderableScene::Camera camera = { };
        camera.eye = inCamera.eye;
        camera.lookAt = inCamera.lookAt;
        camera.right = inCamera.right;
        camera.forward = inCamera.forward;
        camera.up = inCamera.up;
        camera.fov = glm::radians(90.f);
        camera.aspect = 1.f;
        camera.n = inCamera.n;
        camera.f = inCamera.f;
        camera.view = glm::lookAt(camera.eye, camera.lookAt, glm::vec3(0.f, 1.f, 0.f));
        camera.projection = glm::perspective(camera.fov, camera.aspect, camera.n, camera.f);
        traverseBSH(surfelBSH, 0, camera);
        postTraversal(camera, surfelBSH);

        // update texture data using micro buffer
        if (gpuTexture) {
            glTextureSubImage2D(gpuTexture->getGpuObject(), 0, 0, 0, resolution, resolution, GL_RGB, GL_FLOAT, color.data());
        }
    }

    void SoftwareMicroBuffer::visualize(Renderer* renderer, RenderTarget* visRenderTarget) {
        // todo: split the render target into two section
        struct Region {
            i32 x, y;
            i32 width, height;
        };

        Region regionA = { 0, 0, visRenderTarget->width * .5f, visRenderTarget->height };
        Region regionB = { regionA.width, 0, visRenderTarget->width * .5f, visRenderTarget->height };

        Viewport viewport = { };
        glm::uvec2 scaledRes(glm::min(regionA.width * 0.8, regionA.height * 0.8));
        // try to center the micro buffer in the dst traverseBSH target
        viewport.x = regionA.width * .5f - scaledRes.x * .5f;
        viewport.y = regionA.height * .5f - scaledRes.y * .5f;
        viewport.width = scaledRes.x;
        viewport.height = scaledRes.y;

#if 0
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "VisualizeMicroBufferShader",
            SHADER_SOURCE_PATH "visualize_micro_buffer_v.glsl", 
            SHADER_SOURCE_PATH "visualize_micro_buffer_p.glsl", 
            nullptr,
            nullptr,
        });

        renderer->drawScreenQuad(
            visRenderTarget,
            viewport,
            shader,
            [this, visRenderTarget](RenderTarget* renderTarget, Shader* shader) { 
                visRenderTarget->setColorBuffer(visualization, 0);
                visRenderTarget->setDrawBuffers({ 0 });
                visRenderTarget->clearDrawBuffer(0, glm::vec4(0.1f, 0.1f, 0.1f, 1.f));
                shader->setTexture("microColorBuffer", gpuTexture);
                shader->setUniform("microBufferRes", resolution);
            }
        );
#endif
    }

    // micro-rendering on cpu
    void MicroRenderingGI::softwareMicroRendering(const RenderableScene::Camera& inCamera, SurfelBSH& surfelBSH) {
        m_softwareMicroBuffer.render(inCamera, surfelBSH);
        // m_softwareMicroBuffer.raytrace(inCamera, surfelBSH);
    }

    // micro-rendering on gpu
    void MicroRenderingGI::hardwareMicroRendering(const RenderableScene::Camera& camera, SurfelBSH& surfelBSH) {
#if 0
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "MicroRenderingShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "micro_rendering_c.glsl",
        });
        surfelBSH.gpuNodes.bind(64);
        shader->setUniform("view", camera.view);
        shader->setUniform("projection", camera.projection);
        glBindImageTexture(1, debugMicroBuffer->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16);
        m_gfxc->setShader(shader);
        glDispatchCompute(1, 1, 1);
#endif
    }

    void MicroRenderingGI::customUI() {
        PointBasedManyViewGI::customUI();
        ImGui::Checkbox("Visualize Surfel BSH", &bVisualizeSurfelBSH);
        if (bVisualizeSurfelBSH) {
            ImGui::Checkbox("Visualize Bounding Spheres", &m_surfelBSH.bVisualizeBoundingSpheres);
            ImGui::Checkbox("Visualize Nodes", &m_surfelBSH.bVisualizeNodes);
            ImGui::Text("Visualize Level");
            ImGui::SliderInt("##Visualize Level", &m_surfelBSH.activeVisLevel, 0, m_surfelBSH.getNumLevels() - 1);
        }
        ImGui::Checkbox("Visualize Surfel Sampler", &bVisualizeSurfelSampler);
    }
}