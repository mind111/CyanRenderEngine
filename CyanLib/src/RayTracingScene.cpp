
#include "RayTracingScene.h"

namespace Cyan
{
    RayTracingScene::RayTracingScene(const Scene& scene)
        : positionBuffer("PositionBuffer")
        , normalBuffer("NormalBuffer")
        , materialBuffer("MaterialBuffer")
    {
        if (auto perspectiveCamera = dynamic_cast<PerspectiveCamera*>(scene.m_mainCamera->getCamera()))
        {
            camera = *perspectiveCamera;
        }

        std::unordered_map<std::string, u32> meshMap;
        std::unordered_map<std::string, u32> materialMap;

        // reserve a default material at index 0
        m_materials.emplace_back();
        auto defaultMaterial = m_materials.back();

        for (auto entity : scene.m_entities)
        {
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.emplace_back();
                auto& rtxMeshInst = meshInstances.back();
                rtxMeshInst.worldTransform = staticMesh->getWorldTransformMatrix();

                MeshInstance* meshInst = staticMesh->getMeshInstance();
                StaticMesh* mesh = meshInst->mesh;
                auto meshEntry = meshMap.find(mesh->m_name);
                if (meshEntry == meshMap.end())
                {
                    m_meshes.emplace_back();
                    auto& rtxMesh = m_meshes.back();
                    
                    // convert geometry data
                    for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
                    {
                        auto submesh = mesh->getSubmesh(i);
                        auto triMesh = dynamic_cast<Triangles*>(submesh->geometry.get());
                        if (triMesh)
                        {
                            rtxMesh.submeshes.emplace_back();
                            TriangleArray& triangles = rtxMesh.submeshes.back();

                            const auto& vertices = triMesh->vertices;
                            const auto& indices = triMesh->indices;
                            u32 numTriangles = indices.size() / 3;
                            for (u32 f = 0; f < numTriangles; ++f)
                            {
                                if (indices[f * 3] == indices[f * 3 + 1]
                                    || indices[f * 3] == indices[f * 3 + 2]
                                    || indices[f * 3 + 1] == indices[f * 3 + 2])
                                {
                                    continue;
                                }

                                for (u32 v = 0; v < 3; ++v)
                                {
                                    triangles.positions.push_back(vertices[indices[f * 3 + v]].pos);
                                    triangles.normals.push_back(vertices[indices[f * 3 + v]].normal);
                                    triangles.tangents.push_back(vertices[indices[f * 3 + v]].tangent);
                                    triangles.texCoords.push_back(vertices[indices[f * 3 + v]].texCoord0);
                                }
                            }
                        }
                    }

                    meshMap.insert({ mesh->m_name, m_meshes.size() - 1 });
                    rtxMeshInst.parent = m_meshes.size() - 1;
                }
                else
                {
                    rtxMeshInst.parent = meshEntry->second;
                }

                for (u32 i = 0; i < mesh->numSubmeshes(); ++i) {
                    i32 material = 0;
                    // material
                    if (auto matl = meshInst->getMaterial(i)) {
                        auto materialEntry = materialMap.find(matl->m_name);
                        if (materialEntry == materialMap.end())
                        {
                            m_materials.emplace_back();
                            RayTracingMaterial& rtxMaterial = m_materials.back();
                            rtxMaterial.albedo = matl->albedo;
                            rtxMaterial.roughness = matl->roughness;
                            rtxMaterial.metallic = matl->metallic;
                            rtxMaterial.emissive = glm::vec3(matl->emissive * 100.f);

                            materialMap.insert({ matl->m_name, m_materials.size() - 1 });
                            material = m_materials.size() - 1;
                        }
                        else
                        {
                            material = materialEntry->second;
                        }
                    }
                    rtxMeshInst.m_materials.push_back(material);
                }
            }
        }

        /** merge everything into one big triangle array with transform applied, basically converting 
        * mesh instance to a mesh with material applied
        */
        for (auto& meshInst : meshInstances)
        {
            const glm::mat4& model = meshInst.worldTransform;
            const auto& rtxMesh = m_meshes[meshInst.parent];
            for (u32 sm = 0; sm < rtxMesh.submeshes.size(); ++sm)
            {
                const TriangleArray& submesh = rtxMesh.submeshes[sm];
                const RayTracingMaterial& material = m_materials[meshInst.m_materials[sm]];
                for (u32 v = 0; v < submesh.numVerts(); ++v)
                {
                    glm::vec4 positionWS = model * glm::vec4(submesh.positions[v], 1.f);
                    glm::vec4 normalWS = glm::inverse(glm::transpose(model)) * glm::vec4(submesh.normals[v], 1.f);
                    glm::vec4 tangentWS = model * submesh.tangents[v];

                    surfaces.positions.push_back(vec4ToVec3(positionWS));
                    surfaces.normals.push_back(vec4ToVec3(normalWS));
                    surfaces.tangents.push_back(tangentWS);
                    surfaces.texCoords.push_back(submesh.texCoords[v]);

                    positionBuffer.addElement(positionWS);
                    normalBuffer.addElement(normalWS);

                    if (v % 3 == 0)
                    {
                        surfaces.m_materials.push_back(meshInst.m_materials[sm]);
                        GPURayTracingMaterial material = { };
                        material.albedo = glm::vec4(m_materials[meshInst.m_materials[sm]].albedo, 1.f);
                        material.emissive = glm::vec4(m_materials[meshInst.m_materials[sm]].emissive, 1.f);
                        material.roughness = glm::vec4(m_materials[meshInst.m_materials[sm]].roughness);
                        material.metallic = glm::vec4(m_materials[meshInst.m_materials[sm]].metallic);
                        materialBuffer.addElement(material);
                    }
                }
            }
        }

        positionBuffer.upload();
        normalBuffer.upload();
        materialBuffer.upload();
    }
}
