// TODO: Deal with name conflicts with windows header
#include <iostream>
#include <fstream>

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
#include "glfw3.h"
#include "Camera.h"
#include "Scene.h"
#include "Material.h"
#include "MathUtils.h"

/*
    * TODO: 
    * [-] FIXME: Swap out the current ugly skybox 
    * [-] Change viewports layout
    * [-] Code refactoring
    * [-] FIXME: Review shaders (Blinn-phong)
    * [-] FIXME: Review post-processing
    * [-] FIXME: Review imaged-based-lighting
    * [-] Add support for Lightmap, baked lighting
    * [-] Viewport resizing
    * [-] Change viewports layout
    * [-] Add GUI
    *     [-] GUI support for importing meshes and related assets
    *     [-] GUI support for adding instances and editing xforms
    * [-] PBR
*/

#define POSITION 1 << 0
#define NORMAL   1 << 1 
#define UV       1 << 2 
#define TANGENTS 1 << 3 

namespace glm {
    //---- Utilities for loading the scene data from json
    void from_json(const nlohmann::json& j, glm::vec3& v) 
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    void from_json(const nlohmann::json& j, glm::vec4& v) 
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
}

void from_json(const nlohmann::json& j, Transform& t) 
{
    t.translation = j.at("translation").get<glm::vec3>();
    glm::vec4 rotation = j.at("rotation").get<glm::vec4>();
    t.qRot = glm::quat(cos(RADIANS(rotation.x * 0.5f)), sin(RADIANS(rotation.x * 0.5f)) * glm::vec3(rotation.y, rotation.z, rotation.w));
    t.scale = j.at("scale").get<glm::vec3>();
}

void from_json(const nlohmann::json& j, Camera& c) 
{
    c.position = j.at("position").get<glm::vec3>();
    c.lookAt = j.at("lookAt").get<glm::vec3>();
    c.worldUp = j.at("worldUp").get<glm::vec3>();
    j.at("fov").get_to(c.fov);
    j.at("z_far").get_to(c.f);
    j.at("z_near").get_to(c.n);
}

// TODO: Where should initialization happen..?
static CyanRenderer* sRenderer = nullptr;

// TODO: Is this thread-safe?
CyanRenderer* CyanRenderer::get()
{
    if (!sRenderer)
        sRenderer = new CyanRenderer();
    return sRenderer;
}

MeshGroup* CyanRenderer::findMesh(const std::string& desc) 
{
    for(auto& mesh : mRenderAssets.meshes)
    {
        if (desc == mesh->desc)
        {
            return mesh;
        }
    }
    return nullptr;
}

Texture* CyanRenderer::findTexture(const std::string& name) 
{
    for(auto& texture : mRenderAssets.textures)
    {
        if (name == texture->name)
        {
            return texture;
        }
    }
    return nullptr;
}

Material* CyanRenderer::findMaterial(const std::string& name) 
{
    for(auto& matl : mRenderAssets.materials)
    {
        if (name == matl->m_name)
        {
            return matl;
        }
    }
    return nullptr;
}

float findMin(float* vertexData, u32 numVerts, int vertexSize, int offset)
{
    float min = vertexData[offset];
    for (u32 v = 1; v < numVerts; v++)
    {
        min = min(min, vertexData[v * vertexSize + offset]); 
    }
    return min;
}

float findMax(float* vertexData, u32 numVerts, int vertexSize, int offset)
{
    float max = vertexData[offset];
    for (u32 v = 1; v < numVerts; v++)
    {
        max = max(max, vertexData[v * vertexSize + offset]); 
    }
    return max;
}

// TODO: Normalize mesh coordinates to x[-1, 1], y[-1, 1], z[-1, 1]
glm::mat4 ToolKit::normalizeMeshScale(MeshGroup* mesh)
{
    float meshXMin = FLT_MAX; 
    float meshXMax = FLT_MIN;
    float meshYMin = FLT_MAX; 
    float meshYMax = FLT_MIN;
    float meshZMin = FLT_MAX; 
    float meshZMax = FLT_MIN;

    for (auto subMesh : mesh->subMeshes)
    {
        float xMin = findMin(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 0);
        float yMin = findMin(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 1);
        float zMin = findMin(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 2);
        float xMax = findMax(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 0);
        float yMax = findMax(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 1);
        float zMax = findMax(subMesh->vertexInfo.vertexData, subMesh->mNumVerts, subMesh->vertexInfo.vertexSize, 2);
        meshXMin = min(meshXMin, xMin);
        meshYMin = min(meshYMin, yMin);
        meshZMin = min(meshZMin, zMin);
        meshXMax = max(meshXMax, xMax);
        meshYMax = max(meshYMax, yMax);
        meshZMax = max(meshZMax, zMax);
    }
    f32 scale = max(Cyan::fabs(meshXMin), Cyan::fabs(meshXMax));
    scale = max(scale, max(Cyan::fabs(meshYMin), Cyan::fabs(meshYMax)));
    scale = max(scale, max(Cyan::fabs(meshZMin), Cyan::fabs(meshZMax)));
    return glm::scale(glm::mat4(1.f), glm::vec3(1.f / scale));
}

void ToolKit::loadScene(Scene& scene, const char* filename)
{
    nlohmann::json sceneJson;
    std::ifstream sceneFile(filename);
    sceneFile >> sceneJson;
    auto cameras = sceneJson["cameras"];
    auto lights = sceneJson["lights"];
    auto meshInfoList = sceneJson["mesh_info_list"];
    auto textureInfoList = sceneJson["texture_info_list"];
    auto entities = sceneJson["entities"];
    auto materials = sceneJson["materials"];

    // TODO: each scene should only have one camera
    for (auto camera : cameras) 
    {
        scene.mainCamera = camera.get<Camera>();
        scene.mainCamera.view = glm::mat4(1.f);
        scene.mainCamera.projection = glm::perspective(glm::radians(scene.mainCamera.fov), 800.f / 600.f, scene.mainCamera.n, scene.mainCamera.f);
    }

// TODO: Deal with loading lights information
/*
    for (auto lightinfo : lights) 
    {
        std::string lighttype;
        directionallight directionlight;
        pointlight pointlight;
        lightinfo.at("type").get_to(lighttype);
        if (lighttype == "directional") {
            directionlight.direction = glm::normalize(lightinfo.at("direction").get<glm::vec4>().);
            directionlight.baselight.color = lightinfo.at("color").get<glm::vec3>();
            scene.dlights.emplace_back(directionlight);
        } else {

        } // pointLight case
    }
*/

    for (auto textureInfo : textureInfoList) 
    {
        Texture* texture = new Texture();
        texture->path = textureInfo.at("texturePath").get<std::string>();
        texture->name = textureInfo.at("textureName").get<std::string>();
        int numChannels = 0;
        texture->pixels = TextureUtils::loadImage(texture->path.c_str(), &texture->width, &texture->height, &numChannels);

        glCreateTextures(GL_TEXTURE_2D, 1, &texture->id);

        // TODO: How to determin how many mipmap levels there should be ...?
        // Turn off mipmap for now
        GLuint kNumMipLevels = 4;

        // RGB
        if (numChannels == 3)
        {
            glTextureStorage2D(texture->id, kNumMipLevels, GL_RGB8, texture->width, texture->height);
            glTextureSubImage2D(texture->id, 0, 0, 0, texture->width, texture->height, GL_RGB, GL_UNSIGNED_BYTE, texture->pixels);
        }
        // RGBA
        else if (numChannels == 4)
        {
            glTextureStorage2D(texture->id, kNumMipLevels, GL_RGBA8, texture->width, texture->height);
            glTextureSubImage2D(texture->id, 0, 0, 0, texture->width, texture->height, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glGenerateTextureMipmap(texture->id);

        CyanRenderer::get()->mRenderAssets.textures.push_back(texture);
    }

    for (auto meshInfo : meshInfoList) 
    {
        std::string path;
        meshInfo.at("path").get_to(path);
        std::cout << "Start loading mesh: " << path << std::endl;
        MeshGroup* mesh = CyanRenderer::get()->createMeshFromFile(path.c_str());
        std::cout << "Finished loading mesh: " << path << std::endl;
        meshInfo.at("name").get_to(mesh->desc);
        CyanRenderer::get()->mRenderAssets.meshes.push_back(mesh);
    }

    for (auto matlInfo : materials)
    {
        std::string matlType;
        matlInfo.at("type").get_to(matlType);
        if (matlType == "BlinnPhong")
        {
            BlinnPhongMaterial* matl = new BlinnPhongMaterial();
            matlInfo.at("name").get_to(matl->m_name);

            /* Diffuse maps */
            for (auto diffuseMap : matlInfo["diffuse"])
            {
                matl->m_diffuseMaps.push_back(CyanRenderer::get()->findTexture(diffuseMap));
            }

            /* Specular maps */
            for (auto specMap : matlInfo["specular"])
            {
                matl->m_specularMaps.push_back(CyanRenderer::get()->findTexture(specMap));
            }
            
            /* Emission maps */
            for (auto emissionMap : matlInfo["emission"])
            {
                matl->m_emissionMaps.push_back(CyanRenderer::get()->findTexture(emissionMap));
            }

            /* Normal map */
            std::string normalMapName;
            matlInfo.at("normal").get_to(normalMapName);
            matl->m_normalMap = CyanRenderer::get()->findTexture(normalMapName);

            /* Ambient occlusion map */
            std::string aoMapName;
            matlInfo.at("ao").get_to(aoMapName);
            matl->m_aoMap = CyanRenderer::get()->findTexture(aoMapName);

            /* Roughness map */
            std::string roughnessMapName;
            matlInfo.at("roughness").get_to(roughnessMapName);
            matl->m_roughnessMap = CyanRenderer::get()->findTexture(roughnessMapName);

            CyanRenderer::get()->mRenderAssets.materials.push_back(matl);
        }
        else if (matlType == "Pbr")
        {
            PbrMaterial* matl = new PbrMaterial();
            matlInfo.at("name").get_to(matl->m_name);

            /* Diffuse maps */
            for (auto diffuseMap : matlInfo["diffuse"])
            {
                matl->m_diffuseMaps.push_back(CyanRenderer::get()->findTexture(diffuseMap));
            }

            /* Specular maps */
            for (auto specMap : matlInfo["specular"])
            {
                matl->m_specularMaps.push_back(CyanRenderer::get()->findTexture(specMap));
            }
            
            /* Emission maps */
            for (auto emissionMap : matlInfo["emission"])
            {
                matl->m_emissionMaps.push_back(CyanRenderer::get()->findTexture(emissionMap));
            }

            /* Normal map */
            std::string normalMapName;
            matlInfo.at("normal").get_to(normalMapName);
            matl->m_normalMap = CyanRenderer::get()->findTexture(normalMapName);

            /* Ambient occlusion map */
            std::string aoMapName;
            matlInfo.at("ao").get_to(aoMapName);
            matl->m_aoMap = CyanRenderer::get()->findTexture(aoMapName);

            /* Roughness map */
            std::string roughnessMapName;
            matlInfo.at("roughness").get_to(roughnessMapName);
            matl->m_roughnessMap = CyanRenderer::get()->findTexture(roughnessMapName);

            CyanRenderer::get()->mRenderAssets.materials.push_back(matl);
        }
    }

    // TODO: Allow entities to left some fields blank, such as material
    for (auto entityInfo : entities) 
    {
        std::string meshName, matlName;
        entityInfo.at("meshName").get_to(meshName);
        entityInfo.at("matlName").get_to(matlName);
        auto xformInfo = entityInfo.at("xform");
        Transform xform = entityInfo.at("xform").get<Transform>();
        Entity* newEntity = SceneManager::createEntity(scene);
        newEntity->mesh = CyanRenderer::get()->findMesh(meshName); 
        newEntity->matl = CyanRenderer::get()->findMaterial(matlName); 
        newEntity->xform = entityInfo.at("xform").get<Transform>();
        newEntity->position = glm::vec3(0.f);
    }

#if 0
    // TODO: Fix this
    if (sceneJson["hasSkybox"]) {
        std::cout << "the scene contains skybox" << std::endl;
        auto skyboxInfo = sceneJson["skybox"];
        skyboxInfo.at("name").get_to(scene.skybox.name);
        CubemapTexture cube;
        auto texturePaths = skyboxInfo["texturePaths"];
        
        // TODO: fix the enums
        texturePaths.at("front").get_to(cube.textures[CubemapTexture::front].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::front]);

        texturePaths.at("back").get_to(cube.textures[CubemapTexture::back].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::back]);

        texturePaths.at("left").get_to(cube.textures[CubemapTexture::left].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::left]);

        texturePaths.at("right").get_to(cube.textures[CubemapTexture::right].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::right]);

        texturePaths.at("top").get_to(cube.textures[CubemapTexture::top].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::top]);

        texturePaths.at("bottom").get_to(cube.textures[CubemapTexture::bottom].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::bottom]);

        scene.cubemapTextures.emplace_back(cube);

        setupSkybox(scene.skybox, cube);
    }
#endif
}

VertexInfo ToolKit::loadSubMesh(aiMesh* assimpMesh, uint8_t* attribFlag)
{
    VertexInfo result = { };
    int vertexSize = 0;
    *attribFlag = 0;
    if (assimpMesh->HasPositions())
    {
        vertexSize += 3;
        *attribFlag |= POSITION;
    }
    if (assimpMesh->HasNormals())
    {
        vertexSize += 3;
        *attribFlag |= NORMAL;
    }
    if (assimpMesh->HasTextureCoords(0))
    {
        vertexSize += 2; 
        *attribFlag |= UV;
    }
    if (assimpMesh->HasTangentsAndBitangents())
    {
        vertexSize += 6;
        *attribFlag |= TANGENTS;
    }

    result.vertexData = new float[vertexSize * assimpMesh->mNumVertices];
    result.vertexSize = vertexSize;

    /* TODO:
        * For now we don't need index buffer as assimp rearrange vertex buffer
        * in order of faces
    */

    // TODO: This can be abstract into a function taking flag as a argument
    switch (*attribFlag)
    {
        case (POSITION | NORMAL | UV | TANGENTS):
        {
            for (int v = 0; v < assimpMesh->mNumVertices; v++)
            {
                int index = v * vertexSize;
                int offset = 0;
                // Position
                result.vertexData[index + offset++] = assimpMesh->mVertices[v].x;
                result.vertexData[index + offset++] = assimpMesh->mVertices[v].y;
                result.vertexData[index + offset++] = assimpMesh->mVertices[v].z;
                // Normal
                result.vertexData[index + offset++] = assimpMesh->mNormals[v].x;
                result.vertexData[index + offset++] = assimpMesh->mNormals[v].y;
                result.vertexData[index + offset++] = assimpMesh->mNormals[v].z;
                // UV
                result.vertexData[index + offset++] = assimpMesh->mTextureCoords[0][v].x;
                result.vertexData[index + offset++] = assimpMesh->mTextureCoords[0][v].y;
                // Tangents
                result.vertexData[index + offset++] = assimpMesh->mTangents[v].x;
                result.vertexData[index + offset++] = assimpMesh->mTangents[v].y;
                result.vertexData[index + offset++] = assimpMesh->mTangents[v].z;
                result.vertexData[index + offset++] = assimpMesh->mBitangents[v].x;
                result.vertexData[index + offset++] = assimpMesh->mBitangents[v].y;
                result.vertexData[index + offset++] = assimpMesh->mBitangents[v].z;
            }
            break;
        }
        default:
        {
            // TODO: Error message
            break;
        }
    }
    return result;
}

CyanRenderer::CyanRenderer()
{
    shaderCount = 0;
}

// Create an empty mesh that the data will be filled in later
MeshGroup* CyanRenderer::createMesh(const char* meshName)
{
    MeshGroup* mesh = new MeshGroup();
    mRenderAssets.meshes.push_back(mesh);
    mesh->desc = meshName;
    mesh->normalizeXform = glm::mat4(1.f);
    return mesh;
}

// Load mesh data from a file and push it into the asset store held by the renderer
// TODO: Think about removing unused asset maybe?
MeshGroup* CyanRenderer::createMeshFromFile(const char* fileName)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        fileName,
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
    );
/*
        {
            "name": "helmet_mesh",
            "path": "asset/helmet/mesh/helmet.obj"
        },
*/
    // TODO: Handle Sub-meshes
    MeshGroup* mesh = new MeshGroup();

    for (int i = 0; i < scene->mNumMeshes; i++) 
    {
        Mesh* subMesh = new Mesh();
        mesh->subMeshes.push_back(subMesh);

        subMesh->vertexInfo = ToolKit::loadSubMesh(scene->mMeshes[i], &subMesh->mAttribFlag);
        subMesh->setNumVerts(scene->mMeshes[i]->mNumVertices);
        // Bind the vertex buffer to mesh
        subMesh->setVertexBuffer(VertexBuffer::create(subMesh->vertexInfo.vertexData, subMesh->vertexInfo.vertexSize * sizeof(float) * subMesh->mNumVerts));
        // Set buffer layout
        if (subMesh->mAttribFlag & POSITION)
        {
            subMesh->getVertexBuffer()->pushVertexAttribute({"Position", GL_FLOAT, 3, 0});
        }
        if (subMesh->mAttribFlag & NORMAL)
        {
            subMesh->getVertexBuffer()->pushVertexAttribute({"Normal", GL_FLOAT, 3, 0});
        }
        if (subMesh->mAttribFlag & UV)
        {
            subMesh->getVertexBuffer()->pushVertexAttribute({"UV", GL_FLOAT, 2, 0});
        }
        if (subMesh->mAttribFlag & TANGENTS)
        {
            subMesh->getVertexBuffer()->pushVertexAttribute({"Tangent", GL_FLOAT, 3, 0});
            subMesh->getVertexBuffer()->pushVertexAttribute({"Bitangent", GL_FLOAT, 3, 0});
        }
        subMesh->initVertexAttributes();
    }
    // Store the xform for normalizing object space mesh coordinates
    mesh->normalizeXform = ToolKit::normalizeMeshScale(mesh);
    return mesh;
}

void CyanRenderer::registerShader(ShaderBase* shader)
{
    ASSERT(shaderCount < (sizeof(mShaders) / sizeof(mShaders[0])))
    mShaders[shaderCount++] = shader;
}

glm::mat4 CyanRenderer::generateViewMatrix(const Camera& camera)
{
    return glm::lookAt(camera.position, camera.lookAt, camera.up);
}

void CyanRenderer::updateShaderVariables(Entity* entity)
{
    ShaderBase* shader = getMaterialShader(entity->matl->m_type());
    shader->updateShaderVarsForEntity(entity);
}

// TODO: Handle draw MeshGroup better
void CyanRenderer::drawMesh(MeshGroup& mesh)
{
    for (auto subMesh : mesh.subMeshes)
    {
        glBindVertexArray(subMesh->getVertexArray());
        glDrawArrays(GL_TRIANGLES, 0, subMesh->getNumVerts());
    }
}

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

void CyanRenderer::drawEntity(Scene& scene, Entity* e) 
{
    MeshGroup& mesh = *e->mesh;
    Material& material = *e->matl;
    ShaderBase* shader = getMaterialShader(material.m_type());

    // TODO: This should only be enabled for debug build
    // Reload shader if it is modified
    {
        if (fileWasModified(shader->vsInfo.filePath, &shader->vsInfo.lastWriteTime) ||
            fileWasModified(shader->fsInfo.filePath, &shader->fsInfo.lastWriteTime))
        {
                shader->reloadShaderProgram();
        }
    }
    updateShaderVariables(e);
    // TODO: Extract stuff that does not need to be updated for each entity, such as lighting
    shader->prePass();
    drawMesh(mesh);
}

void CyanRenderer::clearBackBuffer()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void CyanRenderer::enableDepthTest()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void CyanRenderer::setClearColor(glm::vec4 color)
{
    glClearColor(color.r, color.g, color.b, color.a);
}

void CyanRenderer::render(Scene& scene, RenderConfig& renderConfg)
{
    /* 
        Rendering passes
    */

    // Skybox pass
    {

    }

    // Terrain pass
    {

    }

    // Draw all the entities
    {
        for (auto entity : scene.entities)
        {
            drawEntity(scene, &entity);
        }
    }

    // Post-processing
    {
        if (renderConfg.bloom)
        {

        }
    }
}

void CyanRenderer::bindSwapBufferCallback(RequestSwapCallback callback)
{
    mSwapBufferCallback = callback;
}

void CyanRenderer::requestSwapBuffers()
{
    mSwapBufferCallback();
}

ShaderBase* CyanRenderer::getShader(ShaderType type)
{
    ASSERT((int)type < shaders.size());
    return shaders[(int)type];
}

// TODO: Use a map to handle mapping
ShaderType materialTypeToShaderType(MaterialType type)
{
    switch (type)
    {
        case MaterialType::BlinnPhong :
        {
            return ShaderType::BlinnPhong;
        }
        case MaterialType::Pbr :
        {
            return ShaderType::Pbr;
        }
        default:
        {
            // Log error
            return ShaderType::None;
        }
    }
}

// TODO: As of right now, I am assuming that each ShaderType only have one unique instance of 
// shader, m_shaders should really be a set instead of vector
ShaderBase* CyanRenderer::getMaterialShader(MaterialType type)
{
    ShaderType shaderType = materialTypeToShaderType(type);
    for (auto shader : mShaders)
    {
        if ((int)shaderType == (int)shader->getShaderType())
        {
            return shader;
        }
    }
    return nullptr;
}

// ++++++++++++ Legacy code +++++++++++++
// ++++++++++++++++++++++++++++++++++++++
#define BLUR_ITERATION 10

float quadVerts[24] = {
    -1.f, -1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f,

    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f
};

FrameBuffer CyanRenderer::createFrameBuffer(int bufferWidth, int bufferHeight, int internalFormat)
{
    FrameBuffer frameBuffer = { };
    glCreateFramebuffers(1, &frameBuffer.fbo);

    // Color render target
    glCreateTextures(GL_TEXTURE_2D, 1, &frameBuffer.colorTarget);
    glBindTexture(GL_TEXTURE_2D, frameBuffer.colorTarget);

    // reserve memory for the color attachment
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bufferWidth, bufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameBuffer.colorTarget, 0);

    // Depth & stencil render target
    glCreateTextures(GL_TEXTURE_2D, 1, &frameBuffer.depthTarget);
    glBindTexture(GL_TEXTURE_2D, frameBuffer.depthTarget);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, bufferWidth, bufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frameBuffer.depthTarget, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Incomplete framebuffer!" << std::endl;
    }
    return frameBuffer;
}

// TODO: @Refactor this shit
void CyanRenderer::initRenderer() {

#if 0
    //---- Shader initialization ----
    // quadShader.init();
    mShaders[(int)ShaderType::BlinnPhong].init();
    //-------------------------------

    //---- Quad render params -------
    glCreateVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, 4 * sizeof(GL_FLOAT), 0); // vertex pos
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 4 * sizeof(GL_FLOAT), (const void*)(2 * sizeof(GL_FLOAT))); // uv
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    //-------------------------------

    glGenFramebuffers(1, &defaultFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

    // Color attachment
    // Maybe switch to use GLuint[]
    glGenTextures(1, &colorBuffer);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); // reserve memory for the color attachment
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);

    // Depth & stencil attachment
    glGenTextures(1, &depthBuffer);
    glBindTexture(GL_TEXTURE_2D, depthBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 800, 600, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // reserve memory for the color attachment
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "incomplete framebuffer!" << std::endl;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //---- HDR framebuffer ----
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glGenTextures(1, &colorBuffer0);
    glBindTexture(GL_TEXTURE_2D, colorBuffer0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 800, 600, 0, GL_RGB, GL_FLOAT, nullptr); // reserve memory for the color attachment
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer0, 0);

    glGenTextures(1, &colorBuffer1);
    glBindTexture(GL_TEXTURE_2D, colorBuffer1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 800, 600, 0, GL_RGB, GL_FLOAT, nullptr); // reserve memory for the color attachment
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, colorBuffer1, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "HDR framebuffer incomplete !" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //-------------------------

    //---- ping-pong framebuffer ----
    glGenFramebuffers(2, pingPongFBO);
    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFBO[i]);

        glGenTextures(1, &pingPongColorBuffer[i]);
        glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 800, 600, 0, GL_RGB, GL_FLOAT, nullptr); // reserve memory for the color attachment
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingPongColorBuffer[i], 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "ping pong framebuffer incomplete !" << std::endl;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //-------------------------------

    //---- MSAA buffer preparation ----
    glGenFramebuffers(1, &intermFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermFBO);
    glGenTextures(1, &intermColorBuffer);
    glBindTexture(GL_TEXTURE_2D, intermColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, intermColorBuffer, 0);

    // glGenTextures(1, &intermDepthBuffer);
    // glBindTexture(GL_TEXTURE_2D, intermDepthBuffer);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 800, 600, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, intermDepthBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "incomplete intermediate framebuffer!" << std::endl;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &MSAAFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
    glGenTextures(1, &MSAAColorBuffer);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAAColorBuffer);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, 800, 600, true);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAAColorBuffer, 0);

    glGenTextures(1, &MSAADepthBuffer);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAADepthBuffer);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT32F, 800, 600, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, MSAADepthBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "incomplete MSAA framebuffer!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    //---------------------------------

    //--- setup floor grid---------
    floorGrid = {
        20, 0, 0, 0, nullptr
    };

    floorGrid.generateVerts();
    floorGrid.initRenderParams();
    //------------------------------
#endif
} 

void CyanRenderer::prepareGridShader(Scene& scene) {
#if 0
    Transform gridXform = {
        glm::vec3(20.f, 20.f, 20.f),
        glm::vec3(0, 0, 0),
        glm::vec3(0, 0, 0.f)
    };
    glm::mat4 gridModelMat = MathUtils::transformToMat4(gridXform);
    mShaders[(int)ShadingMode::grid]->setUniformMat4f("model", glm::value_ptr(gridModelMat));
    mShaders[(int)ShadingMode::grid]->setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    mShaders[(int)ShadingMode::grid]->setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));
#endif
}

void CyanRenderer::drawGrid() {
    //---- Draw floor grid---
    glBindVertexArray(floorGrid.vao);
    glDrawArrays(GL_LINES, 0, floorGrid.numOfVerts);
    glUseProgram(0);
    glBindVertexArray(0);
    //-----------------------
}

// TODO: Cancel out the translation part of view matrix
void CyanRenderer::drawSkybox(Scene& scene) {
#if 0
    mShaders[(int)ShadingMode::cubemap].bindShader();
    glm::mat4 cubeXform(1.f);
    cubeXform = glm::scale(cubeXform, glm::vec3(100.f));
    mShaders[(int)ShadingMode::cubemap].setUniformMat4f("model", glm::value_ptr(cubeXform));
    mShaders[(int)ShadingMode::cubemap].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));
    mShaders[(int)ShadingMode::cubemap].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    glBindVertexArray(scene.skybox.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, scene.skybox.cubmapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindVertexArray(0);
    glUseProgram(0);
#endif
}

void CyanRenderer::setupMSAA() {
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAAColorBuffer, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, MSAADepthBuffer, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

void CyanRenderer::bloomPostProcess() {
#if 0
    //---- Render to hdr fbo--
    //---- Multi-render target---
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    mShaders[(int)ShadingMode::bloom].bindShader();
    GLuint renderTargert[2] = {
        GL_COLOR_ATTACHMENT0, 
        GL_COLOR_ATTACHMENT1
    };
    glDrawBuffers(2, renderTargert);
    //---------------------------

    glBindVertexArray(quadVAO);
    if (enableMSAA) {
        glBindTexture(GL_TEXTURE_2D, intermColorBuffer);
    } else {
    }

    glBindTexture(GL_TEXTURE_2D, colorBuffer);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    //------------------------------------

    //---- Apply blur -----
    for (int itr = 0; itr < BLUR_ITERATION; itr++) {
        //---- Horizontal pass ----
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFBO[itr * 2 % 2]);
        mShaders[(int)ShadingMode::gaussianBlur].bindShader();
        mShaders[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 1);
        mShaders[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 800.f);
        glBindVertexArray(quadVAO);
        if (itr == 0) {
            glBindTexture(GL_TEXTURE_2D, colorBuffer1);
        } else {
            glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[(itr * 2 - 1) % 2]);
        }
        glDrawArrays(GL_TRIANGLES, 0, 6);
        //--------------------- 

        //---- Vertical pass ----
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFBO[(itr * 2 + 1) % 2]);
        mShaders[(int)ShadingMode::gaussianBlur].bindShader();
        mShaders[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 0);
        mShaders[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 600.f);
        glBindVertexArray(quadVAO);
        glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[itr * 2 % 2]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        //--------------------- 
    }

    //---- Blend final output with original hdrFramebuffer ----
    #if 0
    if (enableMSAA) {
        glBindFramebuffer(GL_FRAMEBUFFER, intermFBO);
    } else {
    }
    #endif

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    
    mShaders[(int)ShadingMode::bloomBlend].bindShader();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBuffer0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[1]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    //---------------------------------
    //--------------------- 
#endif
}

void CyanRenderer::blitBackbuffer() {
    //---- Render final output to default fbo--
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // quadShader.bindShader();
    glBindVertexArray(quadVAO);

    glActiveTexture(GL_TEXTURE0);

    #if 0
    if (enableMSAA) {
        glBindTexture(GL_TEXTURE_2D, intermColorBuffer);
    } else {
    }
    #endif

    glBindTexture(GL_TEXTURE_2D, colorBuffer);

    glViewport(0, 0, 400, 300);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[1]);
    glViewport(400, 0, 400, 300);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glViewport(0, 0, 800, 600); // Need to reset the viewport state so that it doesn't affect offscreen MSAA rendering 

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// Grid: Mesh
// Grid: Material

// TODO: Refactor this into a entity as well
void Grid::initRenderParams() {
    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts[0]) * numOfVerts * 3 * 2, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(GL_FLOAT) * 6, 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(GL_FLOAT) * 6, (GLvoid*)(sizeof(GL_FLOAT) * 3));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Grid::printGridVerts() {
    for (int i = 0; i <= gridSize; i++) {
        std::cout << i << ": " << std::endl;
        std::cout << "x: " << verts[(i * 2 * 2) * 3]     << " "
                  << "y: " << verts[(i * 2 * 2) * 3 + 1] << " "
                  << "z: " << verts[(i * 2 * 2) * 3 + 2] << std::endl; 

        std::cout << "x: " << verts[((i * 2 * 2) + 1) * 3] << " "
                  << "y: " << verts[((i * 2 * 2) + 1) * 3 + 1] << " "
                  << "z: " << verts[((i * 2 * 2) + 1) * 3 + 2] << std::endl; 

        std::cout << "x: " << verts[((i * 2 + 1) * 2) * 3]     << " "
                  << "y: " << verts[((i * 2 + 1) * 2) * 3 + 1] << " "
                  << "z: " << verts[((i * 2 + 1) * 2) * 3 + 2] << std::endl;

        std::cout << "x: " << verts[((i * 2 + 1) * 2 + 1) * 3]     << " "
                  << "y: " << verts[((i * 2 + 1) * 2 + 1) * 3 + 1] << " "
                  << "z: " << verts[((i * 2 + 1) * 2 + 1) * 3 + 2] << std::endl;
    }
}


Texture CyanRenderer::createTexture(GLenum target, 
                                    GLint internalFormat,
                                    GLenum pixelFormat, 
                                    GLenum dataType,
                                    void* image,
                                    int width, 
                                    int height)
{
    Texture texture = { }; 
    texture.width = width;
    texture.height = height;

    glGenTextures(1, &texture.id);
    glBindTexture(target, texture.id);
    glTexImage2D(target, 0, internalFormat, texture.width, texture.height, 0, pixelFormat, dataType, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    return texture;
}

Texture CyanRenderer::createTexture(GLenum target, 
                                    GLint internalFormat,
                                    GLenum pixelFormat, 
                                    GLenum dataType,
                                    int width, 
                                    int height)
{
    Texture texture = { }; 
    texture.width = width;
    texture.height = height;

    glGenTextures(1, &texture.id);
    glBindTexture(target, texture.id);
    glTexImage2D(target, 0, internalFormat, texture.width, texture.height, 0, pixelFormat, dataType, 0);
    #include "Common.h"
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    return texture;
}
