#pragma once

#include <vector>

#include "CyanApp.h"
#include "GraphicsSystem.h"
#include "Mesh.h"
#include "Geometry.h"
#include "imgui/imgui.h"
#include "CyanUI.h"
#include "GfxContext.h"

struct EnvMapDebugger
{
    static const u32 kNumMips = 5u;
    // 8 samples and a fixed normal
    static const u32 kNumDebugSamples = 9u;
    static const u32 vertsPerSample = 2u;
    static const u32 f32PerVerts = 4u;
    Uniform* u_sampler;
    Uniform* u_roughness;
    Uniform* u_drawSamples;
    Uniform* u_projection;
    Uniform* u_view;
    Uniform* u_model;
    Uniform* u_cameraView;
    Uniform* u_cameraProjection;
    Shader* m_shader;
    Cyan::RenderTarget* m_renderTargets[kNumMips];
    RegularBuffer* m_buffer;
    Cyan::Texture* m_envMap;
    Cyan::Texture* m_texture;
    Line m_lines[9];

    void init(Cyan::Texture* envMap) 
    { 
        using Cyan::Texture;
        using Cyan::TextureSpec;
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
        TextureSpec spec = { };
        spec.m_type = Texture::Type::TEX_CUBEMAP;
        spec.m_width = envMap->m_width;
        spec.m_height = envMap->m_height;
        spec.m_format = envMap->m_format;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
        m_texture = textureManager->createTextureHDR("EnvmapDebug", spec);
        glGenerateTextureMipmap(m_texture->m_id);
        m_envMap = envMap;
        m_shader = Cyan::createShader("PrefilterSpecularShader", "../../shader/shader_prefilter_specular.vs", "../../shader/shader_prefilter_specular.fs");
        u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
        u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
        u_cameraView = Cyan::createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = Cyan::createUniform("s_projection", Uniform::Type::u_mat4);
        u_roughness = Cyan::createUniform("roughness", Uniform::Type::u_float);
        u_drawSamples = Cyan::createUniform("drawSamples", Uniform::Type::u_float);
        u_sampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_samplerCube);
        u32 mipWidth = m_texture->m_width; 
        u32 mipHeight = m_texture->m_height;
        for (u32 mip = 0; mip < kNumMips; ++mip)
        {
            m_renderTargets[mip] = Cyan::createRenderTarget(mipWidth, mipHeight);
            m_renderTargets[mip]->attachColorBuffer(m_texture, mip);
            mipWidth /= 2u;
            mipHeight /= 2u;
        }
        for (auto& line : m_lines)
        {
            line.init();
        }
    }

    void draw() 
    { 
        // camera
        Camera camera = { };
        camera.position = glm::vec3(0.f);
        camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
        glm::vec3 cameraTargets[] = {
            {1.f, 0.f, 0.f},   // Right
            {-1.f, 0.f, 0.f},  // Left
            {0.f, 1.f, 0.f},   // Up
            {0.f, -1.f, 0.f},  // Down
            {0.f, 0.f, 1.f},   // Front
            {0.f, 0.f, -1.f},  // Back
        }; 
        // TODO: (Min): Need to figure out why we need to flip the y-axis 
        // I thought they should just be vec3(0.f, 1.f, 0.f)
        // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
        glm::vec3 worldUps[] = {
            {0.f, -1.f, 0.f},   // Right
            {0.f, -1.f, 0.f},   // Left
            {0.f, 0.f, 1.f},    // Up
            {0.f, 0.f, -1.f},   // Down
            {0.f, -1.f, 0.f},   // Forward
            {0.f, -1.f, 0.f},   // Back
        };
        Cyan::Mesh* cubeMesh = Cyan::getMesh("cubemapMesh");
        auto gfxc = Cyan::getCurrentGfxCtx();
        // Cache viewport config
        Cyan::Viewport origViewport = gfxc->m_viewport;
        const u32 numMips = 5;
        u32 mipWidth = m_texture->m_width; 
        u32 mipHeight = m_texture->m_height;
        for (u32 mip = 0; mip < numMips; ++mip)
        {
            gfxc->setViewport({ 0u, 0u, mipWidth, mipHeight } );
            for (u32 f = 0; f < 6u; f++)
            {
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniforms
                Cyan::setUniform(u_projection, &camera.projection);
                Cyan::setUniform(u_view, &camera.view);
                Cyan::setUniform(u_roughness, mip * (1.f / kNumMips));
                if (mip == 1)
                {
                    Cyan::setUniform(u_drawSamples, 1.0f);
                }
                else
                {
                    Cyan::setUniform(u_drawSamples, 0.0f);
                }
                gfxc->setDepthControl(Cyan::DepthControl::kDisable);
                gfxc->setRenderTarget(m_renderTargets[mip], (1 << f));
                gfxc->setShader(m_shader);
                // uniforms
                gfxc->setUniform(u_projection);
                gfxc->setUniform(u_view);
                gfxc->setUniform(u_drawSamples);
                gfxc->setUniform(u_roughness);
                // textures
                gfxc->setSampler(u_sampler, 0);
                gfxc->setTexture(m_envMap, 0);
                gfxc->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
                gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
            }
            mipWidth /= 2u;
            mipHeight /= 2u;
            gfxc->reset();
        }
        // Recover the viewport dimensions
        gfxc->setViewport(origViewport);
        gfxc->setDepthControl(Cyan::DepthControl::kEnable);
        // draw debug lines for samples
        {
            f32* vertices = (f32*)_alloca(kNumDebugSamples * vertsPerSample * f32PerVerts * sizeof(f32));
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glGetNamedBufferSubData(m_buffer->m_ssbo, 0, m_buffer->m_totalSize, vertices);
            f32* ptr = vertices;
            for (u32 sample = 0; sample < kNumDebugSamples; ++sample)
            {
                glm::vec3 v0(ptr[0], ptr[1], ptr[2]);
                glm::vec3 v1(ptr[4], ptr[5], ptr[6]);
                m_lines[sample].setVerts(v0, v1).setColor(glm::vec4(0.4f, 0.7f, 0.7f, 1.f)).setViewProjection(u_cameraView, u_cameraProjection); 
                if (sample == kNumDebugSamples - 1)
                {
                    // set a different color for the fixed normal
                    m_lines[sample].setColor(glm::vec4(0.7f, 0.2f, 0.2f, 1.f)); 
                }
                m_lines[sample].draw();
                ptr += vertsPerSample * f32PerVerts;
            }
        }
    }
};

struct BrdfDebugger
{
    Shader* m_shader;
    Cyan::RenderTarget* m_renderTarget;
    Cyan::Texture* m_output; 
    GLuint m_vbo, m_vao; 
    static const u32 kTexWidth = 512u;
    static const u32 kTexHeight = 512u;

    void init()
    {
        using Cyan::Texture;
        using Cyan::TextureSpec;
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
        TextureSpec spec = { };
        spec.m_type = Texture::Type::TEX_2D;
        spec.m_format = Texture::ColorFormat::R16G16B16A16;
        spec.m_width = kTexWidth;
        spec.m_height = kTexHeight;
        spec.m_min = Texture::Filter::LINEAR;
        spec.m_mag = Texture::Filter::LINEAR;
        spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
        spec.m_r = Texture::Wrap::NONE;
        m_output = textureManager->createTextureHDR("integrateBrdf", spec); 
        m_shader = Cyan::createShader("IntegrateBRDFShader", "../../shader/shader_integrate_brdf.vs", "../../shader/shader_integrate_brdf.fs");
        m_renderTarget = Cyan::createRenderTarget(kTexWidth, kTexWidth);
        m_renderTarget->attachColorBuffer(m_output);

        f32 verts[] = {
            -1.f,  1.f, 0.f, 0.f,  1.f,
            -1.f, -1.f, 0.f, 0.f,  0.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
            -1.f,  1.f, 0.f, 0.f,  1.f,
             1.f, -1.f, 0.f, 1.f,  0.f,
             1.f,  1.f, 0.f, 1.f,  1.f
        };

        glCreateBuffers(1, &m_vbo);
        glCreateVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glNamedBufferData(m_vbo, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexArrayAttrib(m_vao, 0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
        glEnableVertexArrayAttrib(m_vao, 1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw()
    {
        auto gfxc = Cyan::getCurrentGfxCtx();
        Cyan::Viewport origViewport = gfxc->m_viewport;
        gfxc->setViewport({ 0u, 0u, kTexWidth, kTexHeight });
        gfxc->setShader(m_shader);
        gfxc->setRenderTarget(m_renderTarget, 0);
        gfxc->setDepthControl(Cyan::DepthControl::kDisable);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gfxc->setViewport(origViewport);
        gfxc->reset();
        gfxc->setDepthControl(Cyan::DepthControl::kEnable);
    }
};

namespace Pbr
{
    void mouseScrollWheelCallback(double xOffset, double yOffset);
};

// TODO:: impl this
class GameState
{

};

// TODO: Can also let CyanApp define a basic beginFrame(), endFrame() where it calls customBeginFrame(), and customEndFrame(),
// and child class override customBegin() and customEnd() to do each application specific stuffs
class PbrApp : public CyanApp
{
public:
    PbrApp();
    ~PbrApp() { }
    virtual void init(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) override;

    static PbrApp* get();

    virtual void beginFrame() override;
    virtual void render() override;
    virtual void endFrame() override;
    virtual void run() override;
    virtual void shutDown() override;
    // tick
    void update();
    // camera control
    void dispatchCameraCommand(struct CameraCommand& command);
    void orbitCamera(Camera& camera, double deltaX, double deltaY);
    void zoomCamera(Camera& camera, double dx, double dy);
    void rotateCamera(double deltaX, double deltaY);
    void switchCamera();
    bool mouseOverUI();

    // main scene viewport
    void drawSceneViewport(); 
    // ui
    void drawEntityPanel();
    void drawDebugWindows();
    void drawLightingWidgets();
    void drawEntityWindow();
    void drawStats();
    void drawRenderSettings();
    void uiDrawEntityGraph(Entity* entity) ;
    RayCastInfo castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize);
    // init
    void initUniforms();
    void initShaders();
    void initHelmetScene();
    void initEnvMaps();

    friend void Pbr::mouseScrollWheelCallback(double xOffset, double yOffset);

    bool bOrbit;
    bool bRayCast;
    RayCastInfo m_mouseRayHitInfo;
    bool bPicking;
    double m_mouseCursorX, m_mouseCursorY;
    Cyan::IrradianceProbe* m_irradianceProbe;
    GLuint m_debugRayAtomicCounter;
    Cyan::LightFieldProbe* m_lightFieldProbe;
    glm::vec3 m_debugRayTracingNormal;

    std::vector<Scene*> m_scenes;
    RegularBuffer* m_debugRayOctBuffer;
    RegularBuffer* m_debugRayWorldBuffer;
    RegularBuffer* m_debugRayBoundryBuffer;
private:
    float m_sampleVertex[(64 + 1) * 4 * 2] = { };
    
    bool bRunning;
    u32 entityOnFocusIdx;
    u32 m_currentScene;
    u32 m_currentProbeIndex;

    // Materials
    Cyan::MaterialInstance* m_droneMatl;
    Cyan::MaterialInstance* m_helmetMatl;
    Cyan::MaterialInstance* m_cubeMatl;
    Cyan::MaterialInstance* m_coneMatl;
    Cyan::MaterialInstance* m_cornellMatl;
    Cyan::MaterialInstance* m_sphereMatl;
    Cyan::MaterialInstance* m_envmapMatl;
    Cyan::MaterialInstance* m_skyMatl;
    Cyan::MaterialInstance* m_blitMatl;
    Cyan::MaterialInstance* m_roomMatl;
    Cyan::MaterialInstance* m_floorMatl;

    // entities
    Entity* m_envMapEntity;
    SceneNode* m_centerNode;

    /* Shaders */
    Shader* m_pbrShader;
    Shader* m_rayTracingShader;
    Cyan::MaterialInstance* m_rayTracingMatl;
    Shader* m_envmapShader;
    Shader* m_blitShader;
    Shader* m_skyShader;

    /* Buffers */
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;

    // RenderTargets
    Cyan::RenderTarget* m_backgroundRenderTarget;

    /* Textures */
    Cyan::Texture* m_rawEnvmap;
    Cyan::Texture* m_envmap;
    Cyan::Texture* m_proceduralSky;
    Cyan::Texture* m_voxelOutput;
    Cyan::Texture* m_shadowMap;
    // a probe that is infinitely distant
    LightProbe m_probe;
    std::vector<LightProbe> m_probes;

    /* Uniforms */
    Uniform* u_kDiffuse;
    Uniform* u_kSpecular;

    // ui
    UI m_ui;
    Entity* m_selectedEntity;
    SceneNode* m_selectedNode;

    // misc
    ImFont* m_font;
    u32 m_debugViewIndex;
    double m_lastFrameDurationInMs;
    // ui interatctions
    bool bDisneyReparam;
    float m_directDiffuseSlider;
    float m_directSpecularSlider;
    float m_indirectDiffuseSlider;
    float m_indirectSpecularSlider;
    float m_directLightingSlider;
    float m_indirectLightingSlider;
    float m_wrap;
    f32   m_roughness;

    // debug parameters
    Line m_debugRay;

    Cyan::GraphicsSystem* m_graphicsSystem;
};