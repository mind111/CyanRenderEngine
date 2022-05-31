#include <fstream>
#include <unordered_map>
#include <stack>

#include "tiny_obj_loader.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "json.hpp"

#include "Asset.h"
#include "CyanAPI.h"
#include "GfxContext.h"
#include "Camera.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    static const u32 kMaxNumUniforms = 256;
    static const u32 kMaxNumShaders = 64;
    static const u32 kMaxNumMaterialTypes = 64;
    static const u32 kMaxNumSceneComponents = 100000;

    static std::vector<Mesh*> s_meshes;
    static SceneComponent s_sceneNodes[kMaxNumSceneComponents] = { };
    static GfxContext* s_gfxc = nullptr;
    static void* s_memory = nullptr;
    static LinearAllocator* s_allocator = nullptr;

    // each material type share same handle as its according shader 
    static Shader* m_shaders[kMaxNumShaders];

    static u32 m_numSceneNodes = 0u;

    void init()
    {

    }

#if 0
    GfxContext* getCurrentGfxCtx()
    {
        CYAN_ASSERT(s_gfxc, "Graphics context was not initialized!");
        return s_gfxc;
    }
#endif

    RegularBuffer* createRegularBuffer(u32 totalSizeInBytes)
    {
        RegularBuffer* buffer = new RegularBuffer;
        buffer->m_totalSize = totalSizeInBytes;
        buffer->m_sizeToUpload = 0u;
        buffer->m_data = nullptr;
        glCreateBuffers(1, &buffer->m_ssbo);
        glNamedBufferData(buffer->m_ssbo, buffer->m_totalSize, nullptr, GL_DYNAMIC_DRAW);
        return buffer;
    }

    VertexBuffer* createVertexBuffer(void* data, u32 sizeInBytes, VertexSpec&& vertexSpec)
    {
        VertexBuffer* vb = new VertexBuffer(data, sizeInBytes, std::move(vertexSpec));
        return vb;
    }

    RenderTarget* createRenderTarget(u32 width, u32 height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->width = width;
        rt->height = height;
        glCreateFramebuffers(1, &rt->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        {
            glCreateRenderbuffers(1, &rt->rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rt->rbo);
            {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, rt->width, rt->height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->rbo);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        rt->validate();
        return rt;
    }

    // depth only render target
    RenderTarget* createDepthOnlyRenderTarget(u32 width, u32 height)
    {
        RenderTarget* rt = new RenderTarget();
        rt->width = width;
        rt->height = height;
        glCreateFramebuffers(1, &rt->fbo);
        return rt;
    }

    void setBuffer(RegularBuffer* _buffer, void* _data, u32 sizeToUpload)
    {
        _buffer->m_data = _data;
        _buffer->m_sizeToUpload = sizeToUpload;
    }

    namespace Toolkit
    {
#if 0
        void computeAABB(Mesh* mesh)
        {
            auto findMin = [](float* vertexData, u32 numVerts, i32 vertexSize, i32 offset) {
                float min = vertexData[offset];
                for (u32 v = 1; v < numVerts; v++)
                    min = Min(min, vertexData[v * vertexSize + offset]); 
                return min;
            };

            auto findMax = [](float* vertexData, u32 numVerts, i32 vertexSize, i32 offset) {
                float max = vertexData[offset];
                for (u32 v = 1; v < numVerts; v++)
                    max = Max(max, vertexData[v * vertexSize + offset]); 
                return max;
            };

            // note(min): FLT_MIN, FLT_MAX returns positive float limits
            float meshXMin =  FLT_MAX; 
            float meshXMax = -FLT_MAX;
            float meshYMin =  FLT_MAX; 
            float meshYMax = -FLT_MAX;
            float meshZMin =  FLT_MAX; 
            float meshZMax = -FLT_MAX;

            for (auto sm : mesh->m_subMeshes)
            {
                u32 vertexSize = 0; 
                for (auto attrib : sm->m_vertexArray->vb->attributes)
                    vertexSize += attrib.m_size;

                float xMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 0);
                float yMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 1);
                float zMin = findMin((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 2);
                float xMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 0);
                float yMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 1);
                float zMax = findMax((float*)sm->m_vertexArray->vb->m_data, sm->m_numVerts, vertexSize, 2);
                meshXMin = Min(meshXMin, xMin);
                meshYMin = Min(meshYMin, yMin);
                meshZMin = Min(meshZMin, zMin);
                meshXMax = Max(meshXMax, xMax);
                meshYMax = Max(meshYMax, yMax);
                meshZMax = Max(meshZMax, zMax);
            }
            // TODO: need to watch out for zmin and zmax as zmin is actually greater than 0 
            // in OpenGL style right-handed coordinate system. This is why meshZMax is put in 
            // pMin while meshZMin is put in pMax.
            mesh->m_aabb.pmin = glm::vec4(meshXMin, meshYMin, meshZMax, 1.0f);
            mesh->m_aabb.pmax = glm::vec4(meshXMax, meshYMax, meshZMin, 1.0f);
            // TODO: cleanup
            mesh->m_aabb.init();
        }

        glm::mat4 computeMeshNormalization(Mesh* mesh)
        {
            computeAABB(mesh);

            float meshXMin = mesh->m_aabb.pmin.x;
            float meshXMax = mesh->m_aabb.pmax.x;
            float meshYMin = mesh->m_aabb.pmin.y;
            float meshYMax = mesh->m_aabb.pmax.y;
            float meshZMin = mesh->m_aabb.pmin.z;
            float meshZMax = mesh->m_aabb.pmax.z;

            f32 scale = Max(Cyan::fabs(meshXMin), Cyan::fabs(meshXMax));
            scale = Max(scale, Max(Cyan::fabs(meshYMin), Cyan::fabs(meshYMax)));
            scale = Max(scale, Max(Cyan::fabs(meshZMin), Cyan::fabs(meshZMax)));
            return glm::scale(glm::mat4(1.f), glm::vec3(1.f / scale));
        }

        // TODO: implement this to handle scale for submeshes correctly
        glm::mat4 computeMeshesNormalization(std::vector<Mesh*> meshes) {
            for (auto& mesh : meshes) {
                continue;
            }
            return glm::mat4(1.f);
        }
#endif
#if 0
        // Load equirectangular map into a cubemap
        Texture* loadEquirectangularMap(const char* _name, const char* _file, bool _hdr)
        {
            auto textureManager = TextureManager::get();
            const u32 kViewportWidth = 1024;
            const u32 kViewportHeight = 1024;
            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

            // Create textures
            Texture* equirectMap, *envmap;
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_numMips = 1u;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::NONE;
            spec.m_t = Texture::Wrap::NONE;
            spec.m_r = Texture::Wrap::NONE;
            spec.m_data = nullptr;
            if (_hdr)
            {
                spec.m_dataType = Texture::DataType::Float;
                equirectMap = textureManager->createTextureHDR(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = textureManager->createTextureHDR(_name, spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                equirectMap = textureManager->createTexture(_name, _file, spec);
                spec.m_type = Texture::Type::TEX_CUBEMAP;
                spec.m_width = kViewportWidth;
                spec.m_height = kViewportHeight;
                spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
                spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
                envmap = textureManager->createTexture(_name, spec);
            }

            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(envmap);
            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("GenCubemapShader", "../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("rawEnvmapSampler", Uniform::Type::u_sampler2D);
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
            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
            for (u32 f = 0; f < 6u; f++)
            {
                // Update view matrix
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniform
                setUniform(u_projection, &camera.projection);
                setUniform(u_view, &camera.view);
                s_gfxc->setDepthControl(DepthControl::kDisable);
                s_gfxc->setRenderTarget(rt, f);
                // Since we are rendering to a framebuffer, we need to configure the viewport 
                // to prevent the texture being stretched to fit the framebuffer's dimension
                s_gfxc->setViewport({ 0, 0, kViewportWidth, kViewportHeight });
                s_gfxc->setShader(shader);
                s_gfxc->setUniform(u_projection);
                s_gfxc->setUniform(u_view);
                s_gfxc->setSampler(u_envmapSampler, 0);
                s_gfxc->setTexture(equirectMap, 0);
                s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                s_gfxc->reset();
            }
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return envmap;
        }

        Texture* prefilterEnvMapDiffuse(const char* _name, Texture* envMap)
        {
            auto textureManager = TextureManager::get();
            const u32 kViewportWidth = 128u;
            const u32 kViewportHeight = 128u;
            Camera camera = { };
            camera.position = glm::vec3(0.f);
            camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 
            // Create textures
            Texture* diffuseIrradianceMap;
            TextureSpec spec;
            spec.m_format = envMap->m_format;
            spec.m_type = Texture::Type::TEX_CUBEMAP;
            spec.m_numMips = 1u;
            spec.m_width = kViewportWidth;
            spec.m_height = kViewportHeight;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            if (spec.m_format == Texture::ColorFormat::R16G16B16 || spec.m_format == Texture::ColorFormat::R16G16B16A16)
            {
                spec.m_dataType = Texture::DataType::Float;
                diffuseIrradianceMap = textureManager->createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                diffuseIrradianceMap = textureManager->createTexture(envMap->m_name.c_str(), spec);
            }
            // Create render targets
            RenderTarget* rt = createRenderTarget(kViewportWidth, kViewportHeight);
            rt->attachColorBuffer(diffuseIrradianceMap);
            // Create shaders and uniforms
            Shader* shader = Cyan::createShader("DiffuseIrradianceShader", "../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_samplerCube);
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

            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
            for (u32 f = 0; f < 6u; f++)
            {
                // Update view matrix
                camera.lookAt = cameraTargets[f];
                camera.worldUp = worldUps[f];
                CameraManager::updateCamera(camera);
                // Update uniform
                setUniform(u_projection, &camera.projection);
                setUniform(u_view, &camera.view);
                s_gfxc->setDepthControl(DepthControl::kDisable);
                s_gfxc->setRenderTarget(rt, f);
                s_gfxc->setViewport({ 0, 0, kViewportWidth, kViewportHeight });
                s_gfxc->setShader(shader);
                s_gfxc->setUniform(u_projection);
                s_gfxc->setUniform(u_view);
                s_gfxc->setSampler(u_envmapSampler, 0);
                s_gfxc->setTexture(envMap, 0);
                s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
            }
            s_gfxc->reset();
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return diffuseIrradianceMap;
        }

        Texture* prefilterEnvmapSpecular(Texture* envMap)
        {
            CYAN_ASSERT(envMap->m_type == Texture::Type::TEX_CUBEMAP, "Cannot prefilter a non-cubemap texture")
            auto textureManager = TextureManager::get();
            Texture* prefilteredEnvMap;
            // HDR
            TextureSpec spec;
            spec.m_type = Texture::Type::TEX_CUBEMAP;
            spec.m_format = envMap->m_format;
            spec.m_numMips = 11u;
            spec.m_width = envMap->m_width;
            spec.m_height = envMap->m_height;
            spec.m_data = nullptr;
            // set the min filter to mipmap_linear as we need to sample from its mipmap
            spec.m_min = Texture::Filter::MIPMAP_LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            if (spec.m_format == Texture::ColorFormat::R16G16B16 || spec.m_format == Texture::ColorFormat::R16G16B16A16)
            {
                spec.m_dataType = Texture::DataType::Float;
                prefilteredEnvMap = textureManager->createTextureHDR(envMap->m_name.c_str(), spec);
            }
            else
            {
                spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                prefilteredEnvMap = textureManager->createTexture(envMap->m_name.c_str(), spec);
            }
            Shader* shader = createShader("PrefilterSpecularShader", "../../shader/shader_prefilter_specular.vs", "../../shader/shader_prefilter_specular.fs");
            Uniform* u_roughness = Cyan::createUniform("roughness", Uniform::Type::u_float);
            Uniform* u_projection = Cyan::createUniform("projection", Uniform::Type::u_mat4);
            Uniform* u_view = Cyan::createUniform("view", Uniform::Type::u_mat4);
            Uniform* u_envmapSampler = Cyan::createUniform("envmapSampler", Uniform::Type::u_samplerCube);
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
            Mesh* cubeMesh = Cyan::getMesh("CubeMesh");
            if (!cubeMesh)
            {
                cubeMesh = Cyan::Toolkit::createCubeMesh("CubeMesh");
            }
            // Cache viewport config
            Viewport origViewport = s_gfxc->m_viewport;
            const u32 kNumMips = 10u;
            u32 mipWidth = prefilteredEnvMap->m_width; 
            u32 mipHeight = prefilteredEnvMap->m_height;
            RenderTarget* rts[kNumMips];;
            for (u32 mip = 0; mip < kNumMips; ++mip)
            {
                rts[mip] = createRenderTarget(mipWidth, mipHeight);
                rts[mip]->attachColorBuffer(prefilteredEnvMap, mip);
                s_gfxc->setViewport({ 0u, 0u, mipWidth, mipHeight });
                for (u32 f = 0; f < 6u; f++)
                {
                    camera.lookAt = cameraTargets[f];
                    camera.worldUp = worldUps[f];
                    CameraManager::updateCamera(camera);
                    // Update uniforms
                    setUniform(u_projection, &camera.projection);
                    setUniform(u_view, &camera.view);
                    setUniform(u_roughness, mip * (1.f / kNumMips));
                    s_gfxc->setDepthControl(DepthControl::kDisable);
                    s_gfxc->setRenderTarget(rts[mip], f);
                    s_gfxc->setShader(shader);
                    // uniforms
                    s_gfxc->setUniform(u_projection);
                    s_gfxc->setUniform(u_view);
                    s_gfxc->setUniform(u_roughness);
                    // textures
                    s_gfxc->setSampler(u_envmapSampler, 0);
                    s_gfxc->setTexture(envMap, 0);
                    s_gfxc->setPrimitiveType(PrimitiveType::TriangleList);
                    s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

                    s_gfxc->drawIndexAuto(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
                }
                mipWidth /= 2u;
                mipHeight /= 2u;
                s_gfxc->reset();
            }
            // Recover the viewport dimensions
            s_gfxc->setViewport(origViewport);
            s_gfxc->setDepthControl(DepthControl::kEnable);
            return prefilteredEnvMap;
        }

        // integrate brdf for specular IBL
        Texture* generateBrdfLUT()
        {
            auto textureManager = TextureManager::get();

            const u32 kTexWidth = 512u;
            const u32 kTexHeight = 512u;
            TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_format = Texture::ColorFormat::R16G16B16A16;
            spec.m_dataType = Texture::DataType::Float;
            spec.m_numMips = 1u;
            spec.m_width = kTexWidth;
            spec.m_height = kTexHeight;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_t = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_r = Texture::Wrap::CLAMP_TO_EDGE;
            spec.m_data = nullptr;
            Texture* outputTexture = textureManager->createTextureHDR("integrateBrdf", spec); 
            Shader* shader = createShader("IntegrateBRDFShader", "../../shader/shader_integrate_brdf.vs", "../../shader/shader_integrate_brdf.fs");
            RenderTarget* rt = createRenderTarget(kTexWidth, kTexWidth);
            rt->attachColorBuffer(outputTexture);
            f32 verts[] = {
                -1.f,  1.f, 0.f, 0.f,  1.f,
                -1.f, -1.f, 0.f, 0.f,  0.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                -1.f,  1.f, 0.f, 0.f,  1.f,
                 1.f, -1.f, 0.f, 1.f,  0.f,
                 1.f,  1.f, 0.f, 1.f,  1.f
            };
            GLuint vbo, vao;
            glCreateBuffers(1, &vbo);
            glCreateVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glNamedBufferData(vbo, sizeof(verts), verts, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glEnableVertexArrayAttrib(vao, 0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), 0);
            glEnableVertexArrayAttrib(vao, 1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (const void*)(3 * sizeof(f32)));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            auto gfxc = getCurrentGfxCtx();
            Viewport origViewport = gfxc->m_viewport;
            gfxc->setViewport({ 0, 0, kTexWidth, kTexHeight } );
            gfxc->setShader(shader);
            gfxc->setRenderTarget(rt, 0);
            glBindVertexArray(vao);
            gfxc->setDepthControl(Cyan::DepthControl::kDisable);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            gfxc->setDepthControl(Cyan::DepthControl::kEnable);
            gfxc->setViewport(origViewport);
            gfxc->reset();

            return outputTexture;
        }
#endif

    } // Toolkit
} // Cyan