#include "RasterGI.h"
#include "CyanRenderer.h"
#include "LightRenderable.h"
#include "AssetManager.h"

namespace Cyan {
    RasterGI::RasterGI(Renderer* renderer, GfxContext* gfxc) 
    : m_renderer(renderer), m_gfxc(gfxc) {

    }

    void RasterGI::initialize() {
        if (!bInitialized) {
            /** note - @min:
            * plus 1 to save room for a debug radiance cube
            */
            maxNumRadianceCubes = irradianceAtlasRes.x * irradianceAtlasRes.y + 1;
            radianceCubes = new RadianceCube[maxNumRadianceCubes];
            radianceCubemaps = new TextureCubeRenderable*[maxNumRadianceCubes];
            jitteredSampleDirections = new glm::vec3[maxNumRadianceCubes];
            for (i32 i = 0; i < maxNumRadianceCubes; ++i) {
                std::string name = "RadianceCubemap_" + std::to_string(i);
                ITextureRenderable::Spec spec = {};
                spec.width = microBufferRes;
                spec.height = microBufferRes;
                spec.type = TEX_CUBE;
                spec.pixelFormat = PF_RGB16F;
                ITextureRenderable::Parameter params = {};
                /** note - @mind:
                * ran into a "incomplete texture" error when setting magnification filter to FM_TRILINEAR
                */
                params.minificationFilter = FM_BILINEAR;
                // todo: based on the rendering output, pay attention to the difference between using point/bilinear sampling
                params.magnificationFilter = FM_BILINEAR;
                params.wrap_r = WM_CLAMP;
                params.wrap_s = WM_CLAMP;
                params.wrap_t = WM_CLAMP;
                radianceCubemaps[i] = new TextureCubeRenderable(name.c_str(), spec, params);
            }
            debugRadianceCubemap = radianceCubemaps[maxNumRadianceCubes - 1];

            // todo: try to improve performance of basic implementation
            {
                ITextureRenderable::Spec spec = { };
                spec.width = sqrt(perFrameWorkload) * microBufferRes;
                spec.height = sqrt(perFrameWorkload) * microBufferRes;
                spec.type = TEX_CUBE;
                spec.pixelFormat = PF_RGB16F;
                ITextureRenderable::Parameter params = { };
                params.minificationFilter = FM_BILINEAR;
                params.magnificationFilter = FM_BILINEAR;
                params.wrap_r = WM_CLAMP;
                params.wrap_s = WM_CLAMP;
                params.wrap_t = WM_CLAMP;
                radianceCubemapMultiView = new TextureCubeRenderable("RadianceCubeAtlas", spec, params);
            }

            glCreateBuffers(1, &radianceCubeBuffer);
            u32 bufferSize = maxNumRadianceCubes * sizeof(RadianceCube);
            glNamedBufferData(radianceCubeBuffer, bufferSize, nullptr, GL_DYNAMIC_COPY);

            ITextureRenderable::Spec radianceAtlasSpec = { };
            radianceAtlasSpec.type = TEX_2D;
            radianceAtlasSpec.width = irradianceAtlasRes.x * resampledMicroBufferRes;
            radianceAtlasSpec.height = irradianceAtlasRes.y * resampledMicroBufferRes;
            radianceAtlasSpec.pixelFormat = PF_RGB16F;
            radianceAtlas = new Texture2DRenderable("RadianceAtlas", radianceAtlasSpec);
            radianceAtlasRenderTarget = createRenderTarget(radianceAtlas->width, radianceAtlas->height);
            radianceAtlasRenderTarget->setColorBuffer(radianceAtlas, 0);

            /** note - @min:
            * it seems imageStore() only supports 1,2,4 channels textures, so using rgba16f here
            */
            ITextureRenderable::Spec irradianceAtlasSpec = { };
            irradianceAtlasSpec.type = TEX_2D;
            irradianceAtlasSpec.width = irradianceAtlasRes.x;
            irradianceAtlasSpec.height = irradianceAtlasRes.y;
            irradianceAtlasSpec.pixelFormat = PF_RGBA16F;
            irradianceAtlas = new Texture2DRenderable("IrradianceAtlas", irradianceAtlasSpec);

            bInitialized = true;
        }
    }

    // todo: 'scene' here really need to be passed in by value instead of by reference so that the RasterGI don't have to 
    // worry about any state changes to the scene that it's working on
    void RasterGI::setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        reset();
        bBuildingRadianceAtlas = true;
        // copy the scene data
        m_scene.reset(new SceneRenderable(scene));
        m_scene->submitSceneData(m_gfxc);

        placeRadianceCubes(depthBuffer, normalBuffer);
    }

    void RasterGI::reset() {
        bBuildingRadianceAtlas = false;
        m_scene.reset(nullptr);
        renderedFrames = 0;
        // clear the radiance atlas
        glm::vec3 skyColor(0.529f, 0.808f, 0.922f);
        // radianceAtlasRenderTarget->clear({ { 0, glm::vec4(skyColor, 1.f) } });
        radianceAtlasRenderTarget->clear({ { 0 } });
    }

    void RasterGI::render(RenderTarget* outRenderTarget) {
        if (bBuildingRadianceAtlas) {
            progressiveBuildRadianceAtlas(*m_scene);
            buildIrradianceAtlas();
        }

        m_renderer->addUIRenderCommand([this]() {
            m_renderer->appendToRenderingTab([this]() {
                ImGui::Begin("raster gi visualization");
                enum class Visualization {
                    kRadiance = 0,
                    kIrradiance,
                    kCount
                };
                static i32 visualization = (i32)Visualization::kRadiance;
                const char* visualizationNames[(i32)Visualization::kCount] = { "Radiance", "Irradiance" };
                ImVec2 imageSize(640, 360);
                ImGui::Combo("Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
                if (visualization == (i32)Visualization::kRadiance) {
                    ImGui::Image((ImTextureID)radianceAtlas->getGpuResource(), ImVec2(640, 360), ImVec2(0, 1), ImVec2(1, 0));
                }
                if (visualization == (i32)Visualization::kIrradiance) {
                    ImGui::Image((ImTextureID)irradianceAtlas->getGpuResource(), ImVec2(640, 360), ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::End();
            });
        });
    }

    void RasterGI::placeRadianceCubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        auto renderer = Renderer::get();
        // clear raster cube buffer
        u32 radianceCubeBufferSize = sizeof(RadianceCube) * maxNumRadianceCubes;
        memset(radianceCubes, 0x0, radianceCubeBufferSize);

        // todo: how to cache spawned raster cubes and avoid placing duplicated raster cubes
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 52, radianceCubeBuffer);

        auto fillRasterShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "FillRasterShader", SHADER_SOURCE_PATH "blit_v.glsl", SHADER_SOURCE_PATH "fill_raster_p.glsl" });
        auto fillRasterRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(irradianceAtlasRes.x, irradianceAtlasRes.y));
        renderer->submitFullScreenPass(
            fillRasterRenderTarget.get(),
            fillRasterShader,
            [this, depthBuffer, normalBuffer](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("outputSize", glm::vec2(renderTarget->width, renderTarget->height));
                shader->setUniform("debugRadianceCubeScreenCoord", debugRadianceCubeScreenCoord);
                shader->setUniform("maxNumRadianceCubes", maxNumRadianceCubes);
                shader->setTexture("depthBuffer", depthBuffer);
                shader->setTexture("normalBuffer", normalBuffer);
            }
        );

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // read back data from gpu
        glGetNamedBufferSubData(radianceCubeBuffer, 0, radianceCubeBufferSize, radianceCubes);

        // read back debug radiance cube
        if (!bLockDebugRadianceCube) {
            debugRadianceCube = radianceCubes[maxNumRadianceCubes - 1];
        }

        // generate jittered sample directions based on radiance cube's normal vector 
        for (i32 i = 0; i < maxNumRadianceCubes; ++i) {
            if (radianceCubes[i].position.w > 0.f) {
                jitteredSampleDirections[i] = m_sampler.sample(radianceCubes[i].normal);
            }
        }

    }

    void RasterGI::sampleRadiance(const RadianceCube& radianceCube, u32 radianceCubeIndex, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap) {
        auto renderer = Renderer::get();
        auto gfxc = renderer->getGfxCtx();

        Shader* rasterGIShader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "RasterGIShader",
            SHADER_SOURCE_PATH "raster_gi_final_gather_v.glsl", 
            SHADER_SOURCE_PATH "raster_gi_final_gather_p.glsl",
        });
        // suppose this only need to be set once
        for (auto light : scene.lights) {
            light->setShaderParameters(rasterGIShader);
        }

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(radianceCubemap->resolution, radianceCubemap->resolution));
        renderTarget->setColorBuffer(radianceCubemap, 0);
        gfxc->setRenderTarget(renderTarget.get());
        gfxc->setDepthControl(DepthControl::kEnable);
        gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        for (i32 face = 0; face < 6; ++face) {
            // skip the bottom face as it doesn't contribute to irradiance at all
            if (face == 3) {
                continue;
            }
            renderTarget->setDrawBuffers({ face });
            renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f));

            // calculate the tangent frame of radiance cube
            glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
            glm::vec3 up = radianceCube.normal;
            if (jitter) {
                up = jitteredSampleDirections[radianceCubeIndex];
            }
            if (abs(up.y) > 0.98) {
                worldUp = glm::vec3(0.f, 0.f, -1.f);
            }
            glm::vec3 right = glm::cross(worldUp, up);
            glm::vec3 forward = glm::cross(up, right);
            glm::mat3 tangentFrame = {
                right,
                up,
                -forward
            };
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
            PerspectiveCamera radianceCubeCam(
                glm::vec3(radianceCube.position),
                glm::vec3(radianceCube.position) + lookAtDir,
                tangentFrame * LightProbeCameras::worldUps[face],
                90.f,
                0.01f,
                32.f,
                1.0f
            );
            gfxc->setShader(rasterGIShader);
            rasterGIShader->setUniform("view", radianceCubeCam.view());
            rasterGIShader->setUniform("projection", radianceCubeCam.projection());
            rasterGIShader->setUniform("receivingNormal", vec4ToVec3(radianceCube.normal));
            rasterGIShader->setUniform("microBufferRes", microBufferRes);
            Renderer::get()->submitSceneMultiDrawIndirect(scene);
        }
    }

    void RasterGI::resampleRadianceCube(TextureCubeRenderable* radianceCubemap, glm::uvec2 index) {
        m_gfxc->setRenderTarget(radianceAtlasRenderTarget);
        Viewport viewport = { index.x * resampledMicroBufferRes, index.y * resampledMicroBufferRes, resampledMicroBufferRes, resampledMicroBufferRes };
        Shader* octMappingShader = ShaderManager::createShader({ ShaderType::kVsPs, "OctMappingShader", SHADER_SOURCE_PATH "oct_mapping_v.glsl", SHADER_SOURCE_PATH "oct_mapping_p.glsl" });
        m_gfxc->setShader(octMappingShader);
        m_renderer->submitScreenQuadPass(
            radianceAtlasRenderTarget,
            viewport,
            octMappingShader,
            [this, radianceCubemap](RenderTarget* renderTarget, Shader* shader) {
                shader->setTexture("srcCubemap", radianceCubemap);
                shader->setUniform("outputResolution", glm::vec2(resampledMicroBufferRes));
            }
        );
    }

    void RasterGI::progressiveBuildRadianceAtlas(SceneRenderable& scene) {
        bool bBuildingComplete = false;
        for (i32 i = 0; i < perFrameWorkload; ++i) {
            i32 radianceCubeIndex = (perFrameWorkload * renderedFrames + i);
            if (radianceCubeIndex < maxNumRadianceCubes) {
                if (radianceCubes[radianceCubeIndex].position.w > 0.f) {
                    i32 x = radianceCubeIndex % irradianceAtlasRes.x;
                    i32 y = radianceCubeIndex / irradianceAtlasRes.x;
                    glm::vec2 jitter = halton23(radianceCubeIndex);
                    sampleRadiance(radianceCubes[radianceCubeIndex], radianceCubeIndex, true, scene, radianceCubemaps[radianceCubeIndex]);
                    resampleRadianceCube(radianceCubemaps[radianceCubeIndex], glm::uvec2(x, y));
                }
            }
            else {
                bBuildingComplete = true;
                break;
            }
        }
        if (bBuildingComplete) {
            renderedFrames = 0;
            bBuildingRadianceAtlas = false;
            m_renderer->gaussianBlur(radianceAtlas, 2, 1.f);
            m_renderer->gaussianBlur(radianceAtlas, 3, 2.f);
            m_renderer->gaussianBlur(radianceAtlas, 4, 3.f);
            m_renderer->gaussianBlur(radianceAtlas, 5, 4.f);
            m_renderer->gaussianBlur(radianceAtlas, 6, 5.f);
        }
        else {
            renderedFrames++;
        }
    }

    void RasterGI::buildIrradianceAtlas() {
        Shader* convolveIrradianceShader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "RasterGIConvolveIrradianceShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "raster_gi_convolve_irradiance_c.glsl",
        });
        m_gfxc->setShader(convolveIrradianceShader);
        convolveIrradianceShader->setUniform("microBufferRes", resampledMicroBufferRes);
        convolveIrradianceShader->setTexture("radianceAtlas", radianceAtlas);
        glBindImageTexture(0, irradianceAtlas->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(irradianceAtlasRes.x, irradianceAtlasRes.y, 1);
    }

    void RasterGI::sampleRadianceMultiView(const SceneRenderable& scene, u32 startIndex, i32 count) {
        Shader* shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsGsPs,
            "RasterGIMultiViewShader",
            SHADER_SOURCE_PATH "raster_gi_final_gather_multiview_v.glsl", 
            SHADER_SOURCE_PATH "raster_gi_final_gather_multiview_p.glsl",
            SHADER_SOURCE_PATH "raster_gi_final_gather_multiview_g.glsl", 
        });
        // suppose this only need to be set once
        for (auto light : scene.lights) {
            light->setShaderParameters(shader);
        }

        const i32 numFaces = 6;
        struct IndirectDrawArrayCommand
        {
            u32  count;
            u32  instanceCount;
            u32  first;
            i32  baseInstance;
        };
        multiViewBuffer.data.array.clear();
        std::vector<glm::vec4> viewports(perFrameWorkload);
        static ShaderStorageBuffer<DynamicSsboData<glm::vec4>> hemicubeNormalBuffer(perFrameWorkload);
        hemicubeNormalBuffer.data.array.clear();
        hemicubeNormalBuffer.data.array.resize(perFrameWorkload);
        for (i32 i = 0; i < count; ++i) {
            const auto& hemicube = radianceCubes[startIndex + i];
            viewports[i] = { (i % (i32)sqrt(perFrameWorkload)) * microBufferRes, (i / (i32)sqrt(perFrameWorkload)) * microBufferRes, microBufferRes, microBufferRes};
            hemicubeNormalBuffer[i] = hemicube.normal;

            for (i32 face = 0; face < numFaces; ++face) {
                // calculate the tangent frame of radiance cube
                glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
                glm::vec3 up = hemicube.normal;
                if (abs(up.y) > 0.98) {
                    worldUp = glm::vec3(0.f, 0.f, -1.f);
                }
                glm::vec3 right = glm::cross(worldUp, up);
                glm::vec3 forward = glm::cross(up, right);
                glm::mat3 tangentFrame = {
                    right,
                    up,
                    -forward
                };
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
                PerspectiveCamera radianceCubeCam(
                    glm::vec3(hemicube.position),
                    glm::vec3(hemicube.position) + lookAtDir,
                    tangentFrame * LightProbeCameras::worldUps[face],
                    90.f,
                    0.01f,
                    32.f,
                    1.0f
                );
                multiViewBuffer.addElement(radianceCubeCam.view());
            }
        }

        multiViewBuffer.update();
        multiViewBuffer.bind(54);
        hemicubeNormalBuffer.update();
        hemicubeNormalBuffer.bind(55);
        // setup multi view viewports
        glViewportArrayv(0, perFrameWorkload, (GLfloat*)(viewports.data()));

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(radianceCubemapMultiView->resolution, radianceCubemapMultiView->resolution));
        renderTarget->setColorBuffer(radianceCubemapMultiView, 0);
        renderTarget->setDrawBuffers({ 0, 1, 2, 3, 4, 5 });
        for (i32 i = 0; i < 6; ++i) {
            renderTarget->clearDrawBuffer(i, glm::vec4(0.f, 0.f, 0.f, 1.f));
        }
        renderTarget->clearDepthBuffer();
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setShader(shader);
        shader->setUniform("numViews", count);
        glm::mat4 projection = glm::perspective(90.f, 1.f, 0.01f, 32.f);
        shader->setUniform("projection", projection);
        m_renderer->submitSceneMultiDrawIndirect(scene);
    }

    void RasterGI::progressiveBuildRadianceAtlasMultiView(SceneRenderable& scene) {
        bool bBuildingComplete = false;
        if (perFrameWorkload * renderedFrames) {

        }
        for (i32 i = 0; i < perFrameWorkload; ++i) {
            i32 radianceCubeIndex = (perFrameWorkload * renderedFrames + i);
            if (radianceCubeIndex < maxNumRadianceCubes) {
                if (radianceCubes[radianceCubeIndex].position.w > 0.f) {
                    i32 x = radianceCubeIndex % irradianceAtlasRes.x;
                    i32 y = radianceCubeIndex / irradianceAtlasRes.x;
                    glm::vec2 jitter = halton23(radianceCubeIndex);
                }
            }
            else {
                bBuildingComplete = true;
                break;
            }
        }
        if (bBuildingComplete) {
            renderedFrames = 0;
            bBuildingRadianceAtlas = false;
            m_renderer->gaussianBlur(radianceAtlas, 2, 1.f);
            m_renderer->gaussianBlur(radianceAtlas, 3, 2.f);
            m_renderer->gaussianBlur(radianceAtlas, 4, 3.f);
            m_renderer->gaussianBlur(radianceAtlas, 5, 4.f);
            m_renderer->gaussianBlur(radianceAtlas, 6, 5.f);
        }
        else {
            renderedFrames++;
        }
    }

    /** note - @mind:
    * since seamless cubemap texture support is not compatible with bindless texture so cubemap texture is not using texture handle, thus can't conveniently 
    * bind bunch of textures all at once, thuse not rendering textured radiance cube at the moment
    */
    // todo: these cubes are not rendered properly
    void RasterGI::debugDrawRadianceCubes(RenderTarget* outRenderTarget) {
        struct DebugCube {
            glm::mat4 transform;
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboData<DebugCube>> debugCubes;

        struct DebugLine {
            glm::mat4 transform[2];
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboData<DebugLine>> debugLines;

        u32 numCubes = maxNumRadianceCubes - 1;
        debugCubes.data.array.resize(numCubes);
        debugLines.data.array.resize(numCubes * 3);

        for (i32 i = 0; i < numCubes; ++i) {
            // calculate the tangent frame of ith radiance cube
            glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
            glm::vec3 up = radianceCubes[i].normal;
            if (abs(up.y) > 0.98) {
                worldUp = glm::vec3(0.f, 0.f, -1.f);
            }
            glm::vec3 right = glm::cross(worldUp, up);
            glm::vec3 forward = glm::cross(up, right);
            glm::mat4 transform = glm::translate(glm::mat4(radianceCubes[i].position.w), vec4ToVec3(radianceCubes[i].position));
            glm::mat4 tangentFrame = {
                glm::vec4(right, 0.f),
                glm::vec4(up, 0.f),
                glm::vec4(-forward, 0.f),
                glm::vec4(0.f, 0.f, 0.f, 1.f),
            };
            transform = transform * tangentFrame;
            transform = glm::scale(transform, glm::vec3(0.05));
            debugCubes[i].transform = transform;
            debugCubes[i].color = glm::vec4(vec4ToVec3(radianceCubes[i].normal) * .5f + .5f, 1.f);

            const f32 lineLength = .1f;
            {
                // right
                glm::vec3 v0 = vec4ToVec3(radianceCubes[i].position);
                glm::vec3 v1 = v0 + right * lineLength;
                glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
                glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
                debugLines[i * 3 + 0].transform[0] = t0;
                debugLines[i * 3 + 0].transform[1] = t1;
                debugLines[i * 3 + 0].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
            }
            {
                // forward
                glm::vec3 v0 = vec4ToVec3(radianceCubes[i].position);
                glm::vec3 v1 = v0 + forward * lineLength;
                glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
                glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
                debugLines[i * 3 + 1].transform[0] = t0;
                debugLines[i * 3 + 1].transform[1] = t1;
                debugLines[i * 3 + 1].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
            }
            {
                // up
                glm::vec3 v0 = vec4ToVec3(radianceCubes[i].position);
                glm::vec3 v1 = v0 + up * lineLength;
                // glm::vec3 v1 = v0 + jitteredSampleDirections[i] * lineLength;
                glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
                glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
                debugLines[i * 3 + 2].transform[0] = t0;
                debugLines[i * 3 + 2].transform[1] = t1;
                debugLines[i * 3 + 2].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
            }
        }

        m_gfxc->setRenderTarget(outRenderTarget);
        m_gfxc->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        m_gfxc->setDepthControl(DepthControl::kEnable);

#if 0
        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        Shader* debugDrawShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawShader", SHADER_SOURCE_PATH "debug_draw_v.glsl", SHADER_SOURCE_PATH "debug_draw_p.glsl" });
        m_gfxc->setShader(debugDrawShader);
        debugCubes.update();
        debugCubes.bind(53);
        m_gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), numCubes);
        glEnable(GL_CULL_FACE);
#endif

        Shader* debugDrawLineShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawLineShader", SHADER_SOURCE_PATH "debug_draw_line_v.glsl", SHADER_SOURCE_PATH "debug_draw_line_p.glsl" });
        m_gfxc->setShader(debugDrawLineShader);
        debugLines.update();
        debugLines.bind(53);
        glDrawArraysInstanced(GL_LINES, 0, debugLines.data.array.size(), numCubes * 3);
    }

    void RasterGI::debugDrawOneRadianceCube(const RadianceCube& radianceCube, TextureCubeRenderable* radianceCubemap, RenderTarget* outRenderTarget) {
        struct DebugCube {
            glm::mat4 transform;
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboData<DebugCube>> debugCubes;

        struct DebugLine {
            glm::mat4 transform[2];
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboData<DebugLine>> debugLines;

        debugCubes.data.array.resize(1);
        // calculate the tangent frame of radiance cube
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = radianceCube.normal;
        if (abs(up.y) > 0.98) {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        glm::vec3 right = glm::cross(worldUp, up);
        glm::vec3 forward = glm::cross(up, right);
        glm::mat4 transform = glm::translate(glm::mat4(radianceCube.position.w), vec4ToVec3(radianceCube.position));
        glm::mat4 tangentFrame = {
            glm::vec4(right, 0.f),
            glm::vec4(up, 0.f),
            glm::vec4(-forward, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f),
        };
        transform = transform * tangentFrame;
        transform = glm::scale(transform, glm::vec3(0.1));
        debugCubes[0].transform = transform;

        debugLines.data.array.resize(3);
        const f32 lineLength = .2f;
        {
            // right
            glm::vec3 v0 = vec4ToVec3(radianceCube.position);
            glm::vec3 v1 = v0 + right * lineLength;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[0].transform[0] = t0;
            debugLines[0].transform[1] = t1;
            debugLines[0].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
        }
        {
            // forward
            glm::vec3 v0 = vec4ToVec3(radianceCube.position);
            glm::vec3 v1 = v0 + forward * lineLength;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[1].transform[0] = t0;
            debugLines[1].transform[1] = t1;
            debugLines[1].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
        }
        {
            // up
            glm::vec3 v0 = vec4ToVec3(radianceCube.position);
            glm::vec3 v1 = v0 + up * lineLength;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[2].transform[0] = t0;
            debugLines[2].transform[1] = t1;
            debugLines[2].color = glm::vec4(0.f, 0.f, 1.f, 1.f);
        }

        m_gfxc->setRenderTarget(outRenderTarget);
        m_gfxc->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });

        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        Shader* debugDrawRadianceCubeShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawRadianceCubeShader", SHADER_SOURCE_PATH "debug_draw_radiance_cube_v.glsl", SHADER_SOURCE_PATH "debug_draw_radiance_cube_p.glsl" });
        m_gfxc->setShader(debugDrawRadianceCubeShader);
        debugDrawRadianceCubeShader->setUniform("microBufferRes", microBufferRes);
        debugDrawRadianceCubeShader->setTexture("debugRadianceCube", radianceCubemap);
        debugCubes.update();
        // todo: refactor ssbo so that no need to manually set binding point anymore
        debugCubes.bind(53);
        m_gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), 1);
        glEnable(GL_CULL_FACE);

        Shader* debugDrawLineShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawLineShader", SHADER_SOURCE_PATH "debug_draw_line_v.glsl", SHADER_SOURCE_PATH "debug_draw_line_p.glsl" });
        m_gfxc->setShader(debugDrawLineShader);
        debugLines.update();
        debugLines.bind(53);
        glDrawArraysInstanced(GL_LINES, 0, debugLines.data.array.size(), 3);
    }

    void RasterGI::visualizeRadianceCube(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer, const SceneRenderable& scene, RenderTarget* outRenderTarget) {
        placeRadianceCubes(depthBuffer, normalBuffer);
        sampleRadiance(debugRadianceCube, maxNumRadianceCubes - 1, false, scene, debugRadianceCubemap);
        debugDrawOneRadianceCube(debugRadianceCube, debugRadianceCubemap, outRenderTarget);
        resampleRadianceCube(debugRadianceCubemap, glm::uvec2(1, 3));

        m_renderer->addUIRenderCommand([this]() {
            m_renderer->appendToRenderingTab([this]() {
                float coord[2] = { debugRadianceCubeScreenCoord.x, debugRadianceCubeScreenCoord.y };
                ImGui::Text("debug radiance cube screen pos:");
                ImGui::SliderFloat2("##debug radiance cube screen pos:", coord, 0.f, 1.f, "%.2f");
                ImGui::Checkbox("lock debug radiance cube", &bLockDebugRadianceCube);
                debugRadianceCubeScreenCoord.x = coord[0];
                debugRadianceCubeScreenCoord.y = coord[1];
                ImGui::Image((ImTextureID)radianceAtlas->getGpuResource(), ImVec2(radianceAtlas->width * 2, radianceAtlas->height * 2), ImVec2(0, 1), ImVec2(1, 0));
            });
        });
    }
}