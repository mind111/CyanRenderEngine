// TODO: Deal with name conflicts with windows header
#include <iostream>
#include <fstream>

#include "glfw3.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtx/quaternion.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "json.hpp"
#include "glm.hpp"

#include "Common.h"
#include "CyanRenderer.h"
#include "Material.h"
#include "MathUtils.h"

bool fileWasModified(const char* fileName, FILETIME* writeTime)
{
    FILETIME lastWriteTime;
    HANDLE hFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    // If the file is being written to then CreatFile will return a invalid handle.
    if (hFile != INVALID_HANDLE_VALUE)
    {
        GetFileTime(hFile, 0, 0, &lastWriteTime);
        CloseHandle(hFile);
        if (CompareFileTime(&lastWriteTime, writeTime) > 0)
        {
            *writeTime = lastWriteTime;
            return true;
        }
    }
    return false;
}

namespace Cyan
{
    void Renderer::init()
    {
        Cyan::init();
        u_model = createUniform("s_model", Uniform::Type::u_mat4);
        u_cameraView = createUniform("s_view", Uniform::Type::u_mat4);
        u_cameraProjection = createUniform("s_projection", Uniform::Type::u_mat4);
        m_frame = new Frame;
        // set back-face culling
        Cyan::getCurrentGfxCtx()->setCullFace(FrontFace::CounterClockWise, FaceCull::Back);
    }

    void Renderer::drawMeshInstance(MeshInstance* meshInstance, glm::mat4* modelMatrix)
    {
        Mesh* mesh = meshInstance->m_mesh;

        for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
        {
            auto ctx = Cyan::getCurrentGfxCtx();
            MaterialInstance* matl = meshInstance->m_matls[i]; 
            Material* matlClass = matl->m_template;
            Shader* shader = matlClass->m_shader;
            // TODO: this is obviously redundant
            ctx->setShader(matlClass->m_shader);
            if (modelMatrix)
            {
                // TODO: this is obviously redundant
                Cyan::setUniform(u_model, &(*modelMatrix)[0][0]);
                ctx->setUniform(u_model);
            }
            ctx->setUniform(u_cameraView);
            ctx->setUniform(u_cameraProjection);
            UsedBindingPoints used = matl->bind();
            Mesh::SubMesh* sm = mesh->m_subMeshes[i];
            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (u32 t = 0; t < used.m_usedTexBindings; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
            // NOTES: reset shader storage binding points
            // TODO: verify if shader storage binding points are global
            for (u32 p = 0; p < used.m_usedBufferBindings; ++p)
            {
                ctx->setBuffer(nullptr, p);
            }
        }
    }

    // TODO:
    void Renderer::submitMesh(Mesh* mesh, glm::mat4 modelTransform)
    {
        for (u32 index = 0; index < mesh->m_subMeshes.size(); ++index)
        {
            DrawCall draw = {};
            // draw.m_uniformBegin = m_frame->m_uniformBuffer.m_pos;
            // MaterialInstance* material = mesh->m_subMeshes[index]->m_matl;
            // for ( : material)
            // {
            //     Cyan::setUniform();
            // }
            // draw.m_uniformEnd = m_frame->m_uniformBuffer.m_pos;

            draw.m_mesh = mesh;
            draw.m_index = index;
            draw.m_modelTransform = modelTransform;
            m_frame->m_renderQueue.push_back(draw);
        }
    }

    void Renderer::renderFrame()
    {
    #if 0
        auto ctx = Cyan::getCurrentGfxCtx();
        for (auto draw : m_frame->m_renderQueue)
        {
            auto sm = draw.m_mesh->m_subMeshes[draw.m_index];
            MaterialInstance* material = sm->m_matl;
            Material* matlClass = material->m_template;
            auto shader = matlClass->m_shader;

            ctx->setShader(shader);
            // TODO: This is dumb
            Cyan::setUniform(u_model, &draw.m_modelTransform[0][0]);
            ctx->setUniform(u_model);
            // for (auto uniform : shader->m_uniforms)
            // {
            //     ctx->setUniform(uniform);
            // }
            for (auto buffer : shader->m_buffers)
            {
                ctx->setBuffer(buffer);
            }

            u32 usedTextureUnit = material->bind();

            ctx->setVertexArray(sm->m_vertexArray);
            ctx->setPrimitiveType(PrimitiveType::TriangleList);
            ctx->drawIndex(sm->m_numVerts);
            // NOTES: reset texture units because texture unit bindings are managed by gl context 
            // it won't change when binding different shaders
            for (int t = 0; t < usedTextureUnit; ++t)
            {
                ctx->setTexture(nullptr, t);
            }
        }
    #endif
    }

    void Renderer::drawEntity(Entity* entity) 
    {
        auto computeModelMatrix = [&]() {
            glm::mat4 model(1.f);
            // model = trans * rot * scale
            model = glm::translate(model, entity->m_xform->translation);
            glm::mat4 rotation = glm::toMat4(entity->m_xform->qRot);
            model *= rotation;
            model = glm::scale(model, entity->m_xform->scale);
            return model;
        };
        glm::mat4 model;
        if (entity->m_xform)
        {
            model = computeModelMatrix();
            drawMeshInstance(entity->m_meshInstance, &model);
        }
        else
        {
            drawMeshInstance(entity->m_meshInstance, nullptr);
        }
    }

    void Renderer::endFrame()
    {
        m_frame->m_renderQueue.clear();
        Cyan::getCurrentGfxCtx()->flip();
    }

    template <typename T>
    u32 sizeofVector(const std::vector<T>& vec)
    {
        CYAN_ASSERT(vec.size() > 0, "empty vector");
        return sizeof(vec[0]) * (u32)vec.size();
    }

    void Renderer::renderScene(Scene* scene)
    {
        // camera
        Camera& camera = scene->mainCamera;
        CameraManager::updateCamera(camera);
        setUniform(u_cameraView, (void*)&camera.view[0]);
        setUniform(u_cameraProjection, (void*)&camera.projection[0]);
        // lights
        if (!scene->pLights.empty())
        {
            setBuffer(scene->m_pointLightsBuffer, scene->pLights.data(), sizeofVector(scene->pLights));
        }
        if (!scene->dLights.empty())
        {
            setBuffer(scene->m_dirLightsBuffer, scene->dLights.data(), sizeofVector(scene->dLights));
        }

        // determine if probe data should be update for this frame
        bool shouldUpdateProbeData = (!scene->m_lastProbe || (scene->m_currentProbe->m_baseCubeMap->m_id != scene->m_lastProbe->m_baseCubeMap->m_id));
        scene->m_lastProbe = scene->m_currentProbe;

        /* 
          TODO: split entities into those has lighting and those does not
          * such as a helmet mesh need to react to lighiting in the scene but a cubemap meshInstance 
          * does not need to.
        */
        // entities 
        u32 debugCounter = 0;
        for (auto entity : scene->entities)
        {
            MeshInstance* meshInstance = entity->m_meshInstance; 
            Material* materialType = meshInstance->m_matls[0]->m_template;
            u32 numSubMeshs = (u32)meshInstance->m_mesh->m_subMeshes.size();
            // update lighting data if necessary
            // TODO: implement these
            auto updateLighting = []()
            {

            };
            auto updateProbe = [&](Scene* scene, MeshInstance* meshInstance)
            {
                u32 numSubMeshs = (u32)meshInstance->m_mesh->m_subMeshes.size();
                if (shouldUpdateProbeData)
                {
                    for (u32 sm = 0; sm < numSubMeshs; ++sm)
                    {
                        // update probe texture bindings
                        meshInstance->m_matls[sm]->bindTexture("irradianceDiffuse", scene->m_currentProbe->m_diffuse);
                        meshInstance->m_matls[sm]->bindTexture("irradianceSpecular", scene->m_currentProbe->m_specular);
                        meshInstance->m_matls[sm]->bindTexture("brdfIntegral", scene->m_currentProbe->m_brdfIntegral);
                    }
                }
            };

            if (materialType->m_dataFieldsFlag && (1 << Material::DataFields::Lit))
            {
                for (u32 sm = 0; sm < numSubMeshs; ++sm)
                {
                    meshInstance->m_matls[sm]->set("numPointLights", sizeofVector(scene->pLights) / (u32)sizeof(PointLight));
                    meshInstance->m_matls[sm]->set("numDirLights",   sizeofVector(scene->dLights) / (u32)sizeof(DirectionalLight));
                }
            }
            // update light probe data if necessary
            if (materialType->m_dataFieldsFlag && (1 << Material::DataFields::Probe))
            {
                updateProbe(scene, meshInstance);
            }
            drawEntity(entity);
        }
    }

    float quadVerts[24] = {
        -1.f, -1.f, 0.f, 0.f,
        1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,

        -1.f, -1.f, 0.f, 0.f,
        1.f, -1.f, 1.f, 0.f,
        1.f,  1.f, 1.f, 1.f
    };
}