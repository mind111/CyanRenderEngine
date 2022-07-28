
#include "RayTracingScene.h"

namespace Cyan
{
    RayTracingScene::RayTracingScene(const Scene& scene)
    {
        if (auto perspectiveCamera = dynamic_cast<PerspectiveCamera*>(scene.camera->getCamera()))
        {
            camera = *perspectiveCamera;
        }

        std::unordered_map<std::string, u32> meshMap;
        std::unordered_map<std::string, u32> materialMap;

        // reserve a default material at index 0
        materials.emplace_back();
        auto defaultMaterial = materials.back();

        SceneView sceneView(scene, scene.camera->getCamera(), EntityFlag_kVisible | EntityFlag_kStatic);
        for (auto entity : scene.entities)
        {
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.emplace_back();
                auto& rtxMeshInst = meshInstances.back();
                rtxMeshInst.worldTransform = staticMesh->getWorldTransformMatrix();

                MeshInstance* meshInst = staticMesh->getMeshInstance();
                Mesh* mesh = meshInst->parent;
                auto meshEntry = meshMap.find(mesh->name);
                if (meshEntry == meshMap.end())
                {
                    meshes.emplace_back();
                    auto& rtxMesh = meshes.back();
                    
                    // convert geometry data
                    for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
                    {
                        if (Mesh::Submesh<Triangles>* submesh = dynamic_cast<Mesh::Submesh<Triangles>*>(mesh->getSubmesh(i)))
                        {
                            rtxMesh.submeshes.emplace_back();
                            TriangleArray& triangles = rtxMesh.submeshes.back();

                            const auto& vertices = submesh->getVertices();
                            const auto& indices = submesh->getIndices();
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

                    meshMap.insert({ mesh->name, meshes.size() - 1 });
                    rtxMeshInst.parent = meshes.size() - 1;
                }
                else
                {
                    rtxMeshInst.parent = meshEntry->second;
                }

                for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
                {
                    i32 material = 0;
                    // material
                    if (auto pbrMaterial = dynamic_cast<PBRMaterial*>(meshInst->getMaterial(i)))
                    {
                        auto materialEntry = materialMap.find(pbrMaterial->name);
                        if (materialEntry == materialMap.end())
                        {
                            materials.emplace_back();
                            RayTracingMaterial& rtxMaterial = materials.back();
                            rtxMaterial.albedo = pbrMaterial->parameter.kAlbedo;
                            rtxMaterial.emissive = pbrMaterial->parameter.kEmissive * 100.f;
                            rtxMaterial.roughness = pbrMaterial->parameter.kRoughness;
                            rtxMaterial.metallic = pbrMaterial->parameter.kMetallic;

                            materialMap.insert({ pbrMaterial->name, materials.size() - 1 });
                            material = materials.size() - 1;
                        }
                        else
                        {
                            material = materialEntry->second;
                        }
                    }
                    rtxMeshInst.materials.push_back(material);
                }
            }
        }

        /** merge everything into one big triangle array with transform applied, basically converting 
        * mesh instance to a mesh with material applied
        */
        for (auto& meshInst : meshInstances)
        {
            const glm::mat4& model = meshInst.worldTransform;
            const auto& rtxMesh = meshes[meshInst.parent];
            for (u32 sm = 0; sm < rtxMesh.submeshes.size(); ++sm)
            {
                const TriangleArray& submesh = rtxMesh.submeshes[sm];
                const RayTracingMaterial& material = materials[meshInst.materials[sm]];
                for (u32 v = 0; v < submesh.numVerts(); ++v)
                {
                    glm::vec4 positionWS = model * glm::vec4(submesh.positions[v], 1.f);
                    glm::vec4 normalWS = glm::inverse(glm::transpose(model)) * glm::vec4(submesh.normals[v], 1.f);
                    glm::vec4 tangentWS = model * submesh.tangents[v];

                    surfaces.positions.push_back(vec4ToVec3(positionWS));
                    surfaces.normals.push_back(vec4ToVec3(normalWS));
                    surfaces.tangents.push_back(tangentWS);
                    surfaces.texCoords.push_back(submesh.texCoords[v]);

                    ADD_SSBO_ELEMENT(positionSsbo, positionWS);
                    ADD_SSBO_ELEMENT(normalSsbo, normalWS);

                    if (v % 3 == 0)
                    {
                        surfaces.materials.push_back(meshInst.materials[sm]);
                        GPURayTracingMaterial material = { };
                        material.albedo = glm::vec4(materials[meshInst.materials[sm]].albedo, 1.f);
                        material.emissive = glm::vec4(materials[meshInst.materials[sm]].emissive, 1.f);
                        material.roughness = glm::vec4(materials[meshInst.materials[sm]].roughness);
                        material.metallic = glm::vec4(materials[meshInst.materials[sm]].metallic);
                        ADD_SSBO_ELEMENT(materialSsbo, material);
                    }
                }
            }
        }

        positionSsbo.update();
        normalSsbo.update();
        materialSsbo.update();
    }
}
