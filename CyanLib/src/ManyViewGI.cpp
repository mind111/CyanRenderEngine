#include "ManyViewGI.h"
#include "CyanRenderer.h"
#include "LightRenderable.h"
#include "AssetManager.h"

namespace Cyan {

    glm::mat4 calcTangentFrame(const glm::vec3& n) {
        // calculate the tangent frame of the input hemicube
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = n;
        if (abs(up.y) > 0.98) {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        glm::vec3 right = glm::cross(worldUp, up);
        glm::vec3 forward = glm::cross(up, right);
        glm::mat4 tangentFrame = {
            glm::vec4(right, 0.f),
            glm::vec4(up, 0.f),
            glm::vec4(-forward, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f),
        };
        return tangentFrame;
    }

    glm::mat4 calcHemicubeTangentFrame(const ManyViewGI::Hemicube& hemicube) {
        // calculate the tangent frame of the input hemicube
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = hemicube.normal;
        if (abs(up.y) > 0.98) {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        glm::vec3 right = glm::cross(worldUp, up);
        glm::vec3 forward = glm::cross(up, right);
        glm::mat4 tangentFrame = {
            glm::vec4(right, 0.f),
            glm::vec4(up, 0.f),
            glm::vec4(-forward, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f),
        };
        return tangentFrame;
    }

    glm::mat4 calcHemicubeTransform(const ManyViewGI::Hemicube& hemicube, const glm::vec3& scale) {
        glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicube);
        glm::mat4 transform = glm::translate(glm::mat4(hemicube.position.w), vec4ToVec3(hemicube.position));
        transform = transform * tangentFrame;
        transform = glm::scale(transform, scale);
        return transform;
    }

    ManyViewGI::ManyViewGI(Renderer* renderer, GfxContext* gfxc) 
    : m_renderer(renderer), m_gfxc(gfxc) {

    }

    void ManyViewGI::initialize() {
        if (!bInitialized) {
            maxNumHemicubes = irradianceAtlasRes.x * irradianceAtlasRes.y;
            hemicubes = new Hemicube[maxNumHemicubes];
            radianceCubemaps = new TextureCubeRenderable*[maxNumHemicubes];
            jitteredSampleDirections = new glm::vec3[maxNumHemicubes];
            for (i32 i = 0; i < maxNumHemicubes; ++i) {
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

            glCreateBuffers(1, &radianceCubeBuffer);
            u32 bufferSize = maxNumHemicubes * sizeof(Hemicube);
            glNamedBufferData(radianceCubeBuffer, bufferSize, nullptr, GL_DYNAMIC_COPY);

            ITextureRenderable::Spec radianceAtlasSpec = { };
            radianceAtlasSpec.type = TEX_2D;
            radianceAtlasSpec.width = irradianceAtlasRes.x * resampledMicroBufferRes;
            radianceAtlasSpec.height = irradianceAtlasRes.y * resampledMicroBufferRes;
            radianceAtlasSpec.pixelFormat = PF_RGBA16F;
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

            ITextureRenderable::Spec spec = {};
            spec.type = TEX_2D;
            spec.width = 1280;
            spec.height = 720;
            spec.pixelFormat = PF_RGB16F;
            visualizeIrradianceBuffer = new Texture2DRenderable("visualizeIrradianceBuffer", spec);

            bInitialized = true;
        }
    }

    // todo: 'scene' here really need to be passed in by value instead of by reference so that the ManyViewGI don't have to 
    // worry about any state changes to the scene that it's working on
    void ManyViewGI::setup(const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        reset();
        bBuildingRadianceAtlas = true;
        // copy the scene data
        m_scene.reset(new SceneRenderable(scene));
        m_scene->submitSceneData(m_gfxc);

        // todo: clear irradiance atlas 

        placeHemicubes(depthBuffer, normalBuffer);
    }

    void ManyViewGI::reset() {
        bBuildingRadianceAtlas = false;
        m_scene.reset(nullptr);
        renderedFrames = 0;
        // clear the radiance atlas
        glm::vec3 skyColor(0.529f, 0.808f, 0.922f);
        // radianceAtlasRenderTarget->clear({ { 0, glm::vec4(skyColor, 1.f) } });
        radianceAtlasRenderTarget->clear({ { 0 } });
    }

    void ManyViewGI::run(RenderTarget* outRenderTarget, const SceneRenderable& scene, Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        render(outRenderTarget);

        if (bShowHemicubes) {
            if (m_scene) {
                visualizeHemicubes(outRenderTarget);
                visualizeIrradiance();
            }
        }

        // ui controls
        m_renderer->addUIRenderCommand([this, scene, depthBuffer, normalBuffer, outRenderTarget]() {
            ImGui::Begin("ManyView GI"); {
                if (ImGui::Button("Build Lighting")) {
                    setup(scene, depthBuffer, normalBuffer);
                }
                enum class Visualization {
                    kRadiance = 0,
                    kIrradiance,
                    kCount
                };
                static i32 visualization = (i32)Visualization::kRadiance;
                const char* visualizationNames[(i32)Visualization::kCount] = { "Radiance", "Irradiance" };

                ImVec2 imageSize(320, 180);
                ImGui::Separator();
                ImGui::Text("Visualization"); ImGui::SameLine();
                ImGui::Combo("##Visualization", &visualization, visualizationNames, (i32)Visualization::kCount);
                if (visualization == (i32)Visualization::kRadiance) {
                    ImGui::Image((ImTextureID)radianceAtlas->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                if (visualization == (i32)Visualization::kIrradiance) {
                    ImGui::Image((ImTextureID)irradianceAtlas->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::Checkbox("Show hemicubes", &bShowHemicubes);
                ImGui::Image((ImTextureID)visualizeIrradianceBuffer->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
            } ImGui::End();
        });
    }

    void ManyViewGI::render(RenderTarget* outRenderTarget) {
        if (bBuildingRadianceAtlas) {
            progressiveBuildRadianceAtlas(*m_scene);
        }
    }

    void ManyViewGI::placeHemicubes(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer) {
        auto renderer = Renderer::get();
        // clear raster cube buffer
        u32 radianceCubeBufferSize = sizeof(Hemicube) * maxNumHemicubes;
        memset(hemicubes, 0x0, radianceCubeBufferSize);

        // todo: how to cache spawned raster cubes and avoid placing duplicated raster cubes
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 52, radianceCubeBuffer);

        auto fillRasterShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "FillRasterShader", SHADER_SOURCE_PATH "blit_v.glsl", SHADER_SOURCE_PATH "fill_raster_p.glsl" });
        auto fillRasterRenderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(irradianceAtlasRes.x, irradianceAtlasRes.y));
        renderer->submitFullScreenPass(
            fillRasterRenderTarget.get(),
            fillRasterShader,
            [this, depthBuffer, normalBuffer](RenderTarget* renderTarget, Shader* shader) {
                shader->setUniform("outputSize", glm::vec2(renderTarget->width, renderTarget->height));
                shader->setUniform("maxNumRadianceCubes", maxNumHemicubes);
                shader->setTexture("depthBuffer", depthBuffer);
                shader->setTexture("normalBuffer", normalBuffer);
            }
        );

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // read back data from gpu
        glGetNamedBufferSubData(radianceCubeBuffer, 0, radianceCubeBufferSize, hemicubes);

        // generate jittered sample directions based on radiance cube's normal vector 
        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            if (hemicubes[i].position.w > 0.f) {
                jitteredSampleDirections[i] = m_sampler.sample(hemicubes[i].normal);
            }
        }

    }

    void ManyViewGI::gatherRadiance(const Hemicube& radianceCube, u32 radianceCubeIndex, bool jitter, const SceneRenderable& scene, TextureCubeRenderable* radianceCubemap) {
        Shader* rasterGIShader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "ManyViewGIShader",
            SHADER_SOURCE_PATH "manyview_gi_final_gather_v.glsl", 
            SHADER_SOURCE_PATH "manyview_gi_final_gather_p.glsl",
        });
        // suppose this only need to be set once
        for (auto light : scene.lights) {
            light->setShaderParameters(rasterGIShader);
        }

        auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(radianceCubemap->resolution, radianceCubemap->resolution));
        renderTarget->setColorBuffer(radianceCubemap, 0);
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        for (i32 face = 0; face < 6; ++face) {
            renderTarget->setDrawBuffers({ face });
            renderTarget->clearDrawBuffer(0, glm::vec4(0.f, 0.f, 0.f, 1.f));

            // skip the bottom face as it doesn't contribute to irradiance at all
            if (face == 3) {
                continue;
            }

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
            m_gfxc->setShader(rasterGIShader);
            rasterGIShader->setUniform("view", radianceCubeCam.view());
            rasterGIShader->setUniform("projection", radianceCubeCam.projection());
            rasterGIShader->setUniform("receivingNormal", vec4ToVec3(radianceCube.normal));
            rasterGIShader->setUniform("microBufferRes", microBufferRes);
            m_renderer->submitSceneMultiDrawIndirect(scene);
        }
    }

    void ManyViewGI::resampleRadiance(TextureCubeRenderable* radianceCubemap, glm::uvec2 index) {
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

    // todo: performance of this pass probably needs some improvements!
    // calculate irradiance for a single hemicube
    void ManyViewGI::convolveIrradiance(const glm::ivec2& hemicubeCoord, TextureCubeRenderable* radianceCubemap) {
        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "ManyViewGIConvolveIrradianceShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "manyview_gi_convolve_irradiance_c.glsl",
        });
        m_gfxc->setShader(shader);
        shader->setUniform("hemicubeCoord", hemicubeCoord);
        shader->setUniform("microBufferRes", microBufferRes);
        shader->setUniform("resampledMicroBufferRes", resampledMicroBufferRes);
        shader->setTexture("radianceCubemap", radianceCubemap);
        glBindImageTexture(0, irradianceAtlas->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(1, 1, 1);
    }

    void ManyViewGI::progressiveBuildRadianceAtlas(SceneRenderable& scene) {
        bool bBuildingComplete = false;
        for (i32 i = 0; i < perFrameWorkload; ++i) {
            i32 hemicubeIndex = (perFrameWorkload * renderedFrames + i);
            if (hemicubeIndex < maxNumHemicubes) {
                if (hemicubes[hemicubeIndex].position.w > 0.f) {
                    i32 x = hemicubeIndex % irradianceAtlasRes.x;
                    i32 y = hemicubeIndex / irradianceAtlasRes.x;
                    glm::vec2 jitter = halton23(hemicubeIndex);
                    gatherRadiance(hemicubes[hemicubeIndex], hemicubeIndex, true, scene, radianceCubemaps[hemicubeIndex]);
                    resampleRadiance(radianceCubemaps[hemicubeIndex], glm::uvec2(x, y));
                    convolveIrradiance(glm::ivec2(x, y), radianceCubemaps[hemicubeIndex]);
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

    void ManyViewGI::convolveIrradianceBatched() {
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

    void ManyViewGI::hierarchicalRefinement() {
        const i32 numLevels = 3;
        glm::vec2 resolution(16, 9);
        for (i32 level = 0; level < numLevels; ++level) {
            if (level > 0) {
                // identify which regions needs to be refined
            }
            else {
                // place hemicubes and rasterize them 
            }
        }
    }

    // visualize all hemicubes
    void ManyViewGI::visualizeHemicubes(RenderTarget* outRenderTarget) {
        static ShaderStorageBuffer<DynamicSsboData<u64>> cubemapHandleBuffer(maxNumHemicubes);
        static ShaderStorageBuffer<DynamicSsboData<glm::mat4>> instanceBuffer(maxNumHemicubes);
        struct Line {
            glm::mat4 v0;
            glm::mat4 v1;
            glm::vec4 color;
        };
        static ShaderStorageBuffer<DynamicSsboData<Line>> lineBuffer(3 * maxNumHemicubes);

        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            instanceBuffer[i] = calcHemicubeTransform(hemicubes[i], glm::vec3(0.05));
            if (glIsTextureHandleResidentARB(radianceCubemaps[i]->glHandle) == GL_FALSE) {
                glMakeTextureHandleResidentARB(radianceCubemaps[i]->glHandle);
            }
            cubemapHandleBuffer[i] = radianceCubemaps[i]->glHandle;
            glm::mat4 tangentFrame = calcHemicubeTangentFrame(hemicubes[i]);
            glm::vec4 colors[3] = {
                glm::vec4(1.f, 0.f, 0.f, 1.f),
                glm::vec4(0.f, 1.f, 0.f, 1.f),
                glm::vec4(0.f, 0.f, 1.f, 1.f)
            };
            for (i32 j = 0; j < 3; ++j) {
                lineBuffer[i * 3 + j].v0 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position));
                lineBuffer[i * 3 + j].v1 = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position + tangentFrame[j] * 0.1f));
                lineBuffer[i * 3 + j].color = colors[j];
            }
        }

        instanceBuffer.upload();
        instanceBuffer.bind(63);
        cubemapHandleBuffer.upload();
        cubemapHandleBuffer.bind(64);
        lineBuffer.upload();
        lineBuffer.bind(65);

        m_gfxc->setRenderTarget(outRenderTarget);
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "VisualizeHemicubesShader",
            SHADER_SOURCE_PATH "debug_show_hemicubes_v.glsl",
            SHADER_SOURCE_PATH "debug_show_hemicubes_p.glsl"
            });
        m_gfxc->setShader(shader);
        shader->setUniform("microBufferRes", microBufferRes);
        auto cube = AssetManager::getAsset<Mesh>("UnitCubeMesh");
        m_gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        // draw hemicubes
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), maxNumHemicubes);
        glEnable(GL_CULL_FACE);

        auto lineShader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "DebugDrawLineShader",
            SHADER_SOURCE_PATH "debug_draw_line_v.glsl",
            SHADER_SOURCE_PATH "debug_draw_line_p.glsl"
            });
        m_gfxc->setShader(lineShader);
        // draw tangent frames
        glDrawArraysInstanced(GL_LINES, 0, 2, maxNumHemicubes * 3);
    }

    void ManyViewGI::visualizeIrradiance() {
        auto disk = AssetManager::getAsset<Mesh>("Disk");
        auto renderTarget = std::unique_ptr<RenderTarget>(
            createRenderTarget(visualizeIrradianceBuffer->width, visualizeIrradianceBuffer->height)
        );
        static ShaderStorageBuffer<DynamicSsboData<glm::vec2>> irradianceCoordBuffer(maxNumHemicubes);
        static ShaderStorageBuffer<DynamicSsboData<glm::mat4>> instanceBuffer(maxNumHemicubes);
        u32 numActiveHemicubes = 0;
        for (i32 i = 0; i < maxNumHemicubes; ++i) {
            if (hemicubes[i].position.w > 0.f) {
                f32 x = (f32)(i % irradianceAtlas->width) / irradianceAtlas->width;
                f32 y = (f32)(i / irradianceAtlas->width) / irradianceAtlas->height;
                irradianceCoordBuffer[numActiveHemicubes] = glm::vec2(x, y);
                glm::mat4 transform = glm::translate(glm::mat4(1.f), vec4ToVec3(hemicubes[i].position));
                transform = transform * calcHemicubeTangentFrame(hemicubes[i]);
                transform = glm::scale(transform, glm::vec3(0.1));
                instanceBuffer[numActiveHemicubes] = transform;
                numActiveHemicubes++;
            }
        }
        instanceBuffer.upload();
        instanceBuffer.bind(67);

        irradianceCoordBuffer.upload();
        irradianceCoordBuffer.bind(66);

        renderTarget->setColorBuffer(visualizeIrradianceBuffer, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer({ 0 }, glm::vec4(.2f, .2f, .2f, 1.f));

        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeIrradianceShader",
            SHADER_SOURCE_PATH "visualize_irradiance_v.glsl",
            SHADER_SOURCE_PATH "visualize_irradiance_p.glsl"
            });
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        shader->setTexture("irradianceBuffer", irradianceAtlas);
        m_gfxc->setShader(shader);
        auto va = disk->getSubmesh(0)->getVertexArray();
        m_gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, numActiveHemicubes);
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), numActiveHemicubes);
        }
    }

    ManyViewGIBatched::ManyViewGIBatched(Renderer* renderer, GfxContext* ctx) 
        : ManyViewGI(renderer, ctx) {

    }

    void ManyViewGIBatched::initialize() {
        ManyViewGI::initialize();

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
            hemicubeMultiView = new TextureCubeRenderable("RadianceCubeAtlas", spec, params);

            /** note - @min:
            * some manual setup is required for this particular framebuffer as it requires    
            * binding an cubemap as layered image, thus the depth cubemap also need to be 
            * bind as a layered image
            */
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &hemicubeMultiViewDepthBuffer);
            glBindTexture(GL_TEXTURE_CUBE_MAP, hemicubeMultiViewDepthBuffer);
            for (i32 i = 0; i < 6; ++i) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, microBufferRes * sqrt(perFrameWorkload), microBufferRes * sqrt(perFrameWorkload), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glCreateFramebuffers(1, &hemicubeMultiViewFramebuffer);
            glNamedFramebufferTexture(hemicubeMultiViewFramebuffer, GL_DEPTH_ATTACHMENT, hemicubeMultiViewDepthBuffer, 0);
            glNamedFramebufferTexture(hemicubeMultiViewFramebuffer, GL_COLOR_ATTACHMENT0, hemicubeMultiView->getGpuResource(), 0);
            GLenum completeness = glCheckNamedFramebufferStatus(hemicubeMultiViewFramebuffer, GL_FRAMEBUFFER);
            if (completeness != GL_FRAMEBUFFER_COMPLETE) {
                if (completeness == GL_FRAMEBUFFER_UNDEFINED) {
                    printf("Framebuffer undefined\n");
                }
                else if (completeness == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) {
                    printf("Framebuffer incomplete attachment\n");
                }
                else if (completeness == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) {
                    printf("Framebuffer incomplete missing attachment\n");
                }
                else if (completeness == GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS) {
                    printf("Framebuffer incomplete layer targets\n");
                }
            }
        }
    }

    void ManyViewGIBatched::render(RenderTarget* outRenderTarget) {

    }

    void ManyViewGIBatched::reset() {

    }

    void ManyViewGIBatched::sampleRadianceMultiView(const SceneRenderable& scene, u32 startIndex, i32 count) {
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
            const auto& hemicube = hemicubes[startIndex + i];
            /** note - @min:
            * it seems for each cubemap face, the origin of uv space always starts at top left corner different from typical 
            * GL screen space coordinates convention where bottom left is the origin.
            */
            viewports[i] = { (i % (i32)sqrt(perFrameWorkload)) * microBufferRes, (i / (i32)sqrt(perFrameWorkload)) * microBufferRes, microBufferRes, microBufferRes};
            hemicubeNormalBuffer[i] = hemicube.normal;

            const i32 numFaces = 6;
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

        multiViewBuffer.upload();
        multiViewBuffer.bind(54);
        hemicubeNormalBuffer.upload();
        hemicubeNormalBuffer.bind(55);
        // setup multi view viewports
        glViewportArrayv(0, perFrameWorkload, (GLfloat*)(viewports.data()));

        glBindFramebuffer(GL_FRAMEBUFFER, hemicubeMultiViewFramebuffer);
        glNamedFramebufferDrawBuffer(hemicubeMultiViewFramebuffer, GL_COLOR_ATTACHMENT0);
        f32 clearDepth = 1.f;
        glClearNamedFramebufferfv(hemicubeMultiViewFramebuffer, GL_DEPTH, 0, &clearDepth);
        glm::vec4 clearColor(0.f, 0.f, 0.f, 1.f);
        glClearNamedFramebufferfv(hemicubeMultiViewFramebuffer, GL_COLOR, 0, &clearColor.x);

        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_gfxc->setShader(shader);
        shader->setUniform("numViews", count);
        glm::mat4 projection = glm::perspective(90.f, 1.f, 0.01f, 32.f);
        shader->setUniform("projection", projection);
        m_gfxc->setDepthControl(DepthControl::kEnable);
        m_renderer->submitSceneMultiDrawIndirect(scene);
        
        // resample this multiview hemicube into the radiance atlas
        Shader* resampleMultiViewShader = ShaderManager::createShader({ 
            ShaderSource::Type::kCs,
            "ResampleMultiViewShader",
            nullptr,
            nullptr,
            nullptr,
            SHADER_SOURCE_PATH "resample_radiance_multiview_c.glsl",
        });

#if 0
        static ShaderStorageBuffer<DynamicSsboData<glm::ivec2>> hemicubeIndexBuffer;
        hemicubeIndexBuffer.data.array.clear();
        for (i32 i = 0; i < count; ++i) {
            hemicubeIndexBuffer.addElement(glm::ivec2((startIndex + i) % irradianceAtlasRes.x, (startIndex + i) / irradianceAtlasRes.x));
        }
        hemicubeIndexBuffer.update();
        hemicubeIndexBuffer.bind(56);
        m_gfxc->setShader(resampleMultiViewShader);
        resampleMultiViewShader->setTexture("hemicubeMultiView", hemicubeMultiView);
        glBindImageTexture(0, radianceAtlas->getGpuResource(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glDispatchCompute(sqrt(perFrameWorkload), sqrt(perFrameWorkload), 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#endif
    }

    void ManyViewGIBatched::debugDrawHemicubeMultiView(u32 batchId, u32 index, RenderTarget* outRenderTarget) {
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
        const auto& hemicube = hemicubes[batchId * perFrameWorkload + index];
        debugCubes.data.array.resize(1);
        // calculate the tangent frame of radiance cube
        glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 up = hemicube.normal;
        if (abs(up.y) > 0.98) {
            worldUp = glm::vec3(0.f, 0.f, -1.f);
        }
        glm::vec3 right = glm::cross(worldUp, up);
        glm::vec3 forward = glm::cross(up, right);
        glm::mat4 transform = glm::translate(glm::mat4(hemicube.position.w), vec4ToVec3(hemicube.position));
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
            glm::vec3 v0 = vec4ToVec3(hemicube.position);
            glm::vec3 v1 = v0 + right * lineLength;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[0].transform[0] = t0;
            debugLines[0].transform[1] = t1;
            debugLines[0].color = glm::vec4(1.f, 0.f, 0.f, 1.f);
        }
        {
            // forward
            glm::vec3 v0 = vec4ToVec3(hemicube.position);
            glm::vec3 v1 = v0 + forward * lineLength;
            glm::mat4 t0 = glm::translate(glm::mat4(1.f), v0);
            glm::mat4 t1 = glm::translate(glm::mat4(1.f), v1);
            debugLines[1].transform[0] = t0;
            debugLines[1].transform[1] = t1;
            debugLines[1].color = glm::vec4(0.f, 1.f, 0.f, 1.f);
        }
        {
            // up
            glm::vec3 v0 = vec4ToVec3(hemicube.position);
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
        Shader* shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs, 
            "DebugDrawHemicubeMultiView", 
            SHADER_SOURCE_PATH "debug_draw_radiance_cube_v.glsl", 
            SHADER_SOURCE_PATH "debug_validate_hemicube_multiview_p.glsl" 
        });
        m_gfxc->setShader(shader);
        shader->setTexture("hemicubeMultiView", hemicubeMultiView);
        shader->setUniform("microBufferRes", microBufferRes);
        shader->setUniform("hemicubeIndex", glm::ivec2(index % 4, index / 4));
        debugCubes.upload();
        // todo: refactor ssbo so that no need to manually set binding point anymore
        debugCubes.bind(53);
        m_gfxc->setVertexArray(cube->getSubmesh(0)->getVertexArray());
        glDisable(GL_CULL_FACE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, cube->getSubmesh(0)->numVertices(), 1);
        glEnable(GL_CULL_FACE);

        Shader* debugDrawLineShader = ShaderManager::createShader({ ShaderSource::Type::kVsPs, "DebugDrawLineShader", SHADER_SOURCE_PATH "debug_draw_line_v.glsl", SHADER_SOURCE_PATH "debug_draw_line_p.glsl" });
        m_gfxc->setShader(debugDrawLineShader);
        debugLines.upload();
        debugLines.bind(53);
        glDrawArraysInstanced(GL_LINES, 0, debugLines.data.array.size(), 3);
    }

    void ManyViewGIBatched::visualizeHemicubeMultiView(Texture2DRenderable* depthBuffer, Texture2DRenderable* normalBuffer, const SceneRenderable& scene, RenderTarget* outRenderTarget) {
        i32 centerHemicubeId = (irradianceAtlasRes.y / 2) * irradianceAtlasRes.x + irradianceAtlasRes.x / 2;
        i32 batchId = centerHemicubeId / perFrameWorkload;
        static bool lock = false;
        if (!lock) {
            placeHemicubes(depthBuffer, normalBuffer);
        }
        sampleRadianceMultiView(scene, batchId * perFrameWorkload, perFrameWorkload);
        debugDrawHemicubeMultiView(batchId, 0, outRenderTarget);
        const auto& selectedHemicube = hemicubes[batchId * perFrameWorkload];
        m_renderer->addUIRenderCommand([]() {

        });
    }

    MicroRenderingGI::MicroRenderingGI(Renderer* renderer, GfxContext* gfxc)
        : m_renderer(renderer), m_gfxc(gfxc) {

    }

    void MicroRenderingGI::initialize() { 
        if (!bInitialized) {
            if (!visualizeSurfelBuffer) {
                ITextureRenderable::Spec spec = {};
                spec.type = TEX_2D;
                spec.width = 1280;
                spec.height = 720;
                spec.pixelFormat = PF_RGB16F;
                visualizeSurfelBuffer = new Texture2DRenderable("VisualizeSurfelBuffer", spec);
            }
            if (!surfelSceneColor) {
                ITextureRenderable::Spec spec = {};
                spec.type = TEX_2D;
                spec.width = 1280;
                spec.height = 720;
                spec.pixelFormat = PF_RGB16F;
                surfelSceneColor = new Texture2DRenderable("SurfelSceneColor", spec);
            }
            bInitialized = true;
        }
    }

    void MicroRenderingGI::setup(const SceneRenderable& scene) {
        m_scene.reset(new SceneRenderable(scene));
        generateWorldSpaceSurfels();
    }

    void MicroRenderingGI::run(const SceneRenderable& scene) {
        static bool bVisualizeSurfels = false;
        if (bVisualizeSurfels) {
            if (m_scene) {
                visualizeSurfels();

                auto renderTarget = std::unique_ptr<RenderTarget>(createRenderTarget(surfelSceneColor->width, surfelSceneColor->height));
                renderTarget->setColorBuffer(surfelSceneColor, 0);
                renderTarget->setDrawBuffers({ 0 });
                renderTarget->clearDrawBuffer(0, glm::vec4(0.2f, .2f, .2f, 1.f));
                rasterizeSurfelScene(renderTarget.get(), scene.camera.view, scene.camera.projection);
            }
        }

        // ui controls
        m_renderer->addUIRenderCommand([this, scene]() {
            ImGui::Begin("MicroRendering GI"); {
                if (ImGui::Button("Setup")) {
                    setup(scene);
                }
                ImGui::Checkbox("Visualize Surfels", &bVisualizeSurfels);
                ImVec2 imageSize(320, 180);
                ImGui::Image((ImTextureID)visualizeSurfelBuffer->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image((ImTextureID)surfelSceneColor->getGpuResource(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
            } ImGui::End();
        });
    }

    // todo: implement this on gpu using geometry shader
    void MicroRenderingGI::generateWorldSpaceSurfels() {
        const u32 kMaxNumSurfels = 128 * 128;
        // fixed the surfel radius at 10 centimeters
        const f32 surfelRadius = .1f;

        surfels.clear();
        gpuSurfelBuffer.data.array.clear();
        // calculate total surface area by summing area of all triangles in the scene
        for (i32 i = 0; i < m_scene->meshInstances.size(); ++i) {
            auto meshInst = m_scene->meshInstances[i];
            auto mesh = meshInst->parent;
            glm::mat4 transform = (*m_scene->transformSsbo)[i];
            glm::mat4 normalTransform = glm::inverse(glm::transpose(transform));
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                auto submesh = dynamic_cast<Mesh::Submesh<Triangles>*>(mesh->getSubmesh(sm));
                if (submesh) {
                    glm::vec3 albedo(0.f);
                    auto matl = dynamic_cast<PBRMaterial*>(meshInst->getMaterial(sm));
                    // todo: figure out how to handle textured material
                    if (matl) {
                        albedo = matl->parameter.kAlbedo;
                    }
                    const auto& verts = submesh->getVertices();
                    const auto& indices = submesh->getIndices();
                    i32 numTriangles = submesh->numIndices() / 3;
                    for (i32 t = 0; t < numTriangles; ++t) {
                        const auto& v0 = verts[indices[t * 3 + 0]];
                        const auto& v1 = verts[indices[t * 3 + 1]];
                        const auto& v2 = verts[indices[t * 3 + 2]];

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
                        i32 numSurfels = area / (3.1415926f * surfelRadius * surfelRadius);
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
                            });
                            gpuSurfelBuffer.addElement(GpuSurfel{
                                glm::vec4(st.x * p0 + st.y * p1 + w * p2, 1.f),
                                glm::vec4(glm::normalize(st.x * n0 + st.y * n1 + w * n2), 1.f),
                                glm::vec4(albedo, 1.f)
                            });
                        }
                    }
                }
            }
        }
        gpuSurfelBuffer.upload();

        instanceBuffer.data.array.resize(surfels.size());
        for (i32 i = 0; i < surfels.size(); ++i) {
            glm::mat4 transform = glm::translate(glm::mat4(1.f), surfels[i].position);
            transform = transform * calcTangentFrame(surfels[i].normal);
            instanceBuffer[i].transform = glm::scale(transform, glm::vec3(.1f));
            instanceBuffer[i].color = glm::vec4(surfels[i].color, 1.f);
        }
        instanceBuffer.upload();
    }

    void MicroRenderingGI::rasterizeSurfelScene(RenderTarget* outRenderTarget, const glm::mat4& view, const glm::mat4& projection) {
        auto shader = ShaderManager::createShader({
            ShaderSource::Type::kVsPs,
            "RasterizeSurfelShader",
            SHADER_SOURCE_PATH "rasterize_surfels_v.glsl",
            SHADER_SOURCE_PATH "rasterize_surfels_p.glsl"
        });

        assert(surfels.size() == gpuSurfelBuffer.getNumElements());

        gpuSurfelBuffer.bind(69);

        m_gfxc->setShader(shader);
        m_gfxc->setRenderTarget(outRenderTarget);
        m_gfxc->setViewport({ 0, 0, outRenderTarget->width, outRenderTarget->height });
        shader->setUniform("outputSize", glm::ivec2(outRenderTarget->width, outRenderTarget->height));
        shader->setUniform("cameraFov", 90.f);
        shader->setUniform("cameraN", 0.1f);
        shader->setUniform("surfelRadius", 0.1f);
        shader->setUniform("view", view);
        shader->setUniform("projection", projection);
        shader->setUniform("receivingNormal", m_scene->camera.view[1]);
        glDrawArraysInstanced(GL_POINTS, 0, 1, surfels.size());
    }

    // todo: refactor all debug visualization
    void MicroRenderingGI::visualizeSurfels() {
        instanceBuffer.bind(68);

        auto disk = AssetManager::getAsset<Mesh>("Disk");
        auto renderTarget = std::unique_ptr<RenderTarget>(
            createRenderTarget(visualizeSurfelBuffer->width, visualizeSurfelBuffer->height)
        );
        renderTarget->setColorBuffer(visualizeSurfelBuffer, 0);
        renderTarget->setDrawBuffers({ 0 });
        renderTarget->clearDrawBuffer({ 0 }, glm::vec4(.2f, .2f, .2f, 1.f));

        auto shader = ShaderManager::createShader({ 
            ShaderSource::Type::kVsPs,
            "VisualizeIrradianceShader",
            SHADER_SOURCE_PATH "visualize_surfel_v.glsl",
            SHADER_SOURCE_PATH "visualize_surfel_p.glsl"
            });
        m_gfxc->setRenderTarget(renderTarget.get());
        m_gfxc->setViewport({ 0, 0, renderTarget->width, renderTarget->height });
        m_gfxc->setShader(shader);
        auto va = disk->getSubmesh(0)->getVertexArray();
        m_gfxc->setVertexArray(va);
        if (va->hasIndexBuffer()) {
            glDrawElementsInstanced(GL_TRIANGLES, disk->getSubmesh(0)->numIndices(), GL_UNSIGNED_INT, 0, surfels.size());
        }
        else {
            glDrawArraysInstanced(GL_TRIANGLES, 0, disk->getSubmesh(0)->numVertices(), surfels.size());
        }
    }
}