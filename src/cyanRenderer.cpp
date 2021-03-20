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

namespace Cyan
{
    // TODO: impl
    void createVsShader() { }
    void createCsShader() { }
    void createPsShader() { }

    Shader* createShader(const char* vertSrc, const char* fragSrc)
    {
        // TODO: Memory management
        Shader* shader = new Shader();

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        shader->m_programId = glCreateProgram();

        // Load shader source
        const char* vertShaderSrc = ShaderUtil::readShaderFile(vertSrc);
        const char* fragShaderSrc = ShaderUtil::readShaderFile(fragSrc);
        glShaderSource(vs, 1, &vertShaderSrc, nullptr);
        glShaderSource(fs, 1, &fragShaderSrc, nullptr);

        // Compile shader
        glCompileShader(vs);
        glCompileShader(fs);
        ShaderUtil::checkShaderCompilation(vs);
        ShaderUtil::checkShaderCompilation(fs);

        // Link shader
        glAttachShader(shader->m_programId, vs);
        glAttachShader(shader->m_programId, fs);
        glLinkProgram(shader->m_programId);
        ShaderUtil::checkShaderLinkage(shader->m_programId);

        glDeleteShader(vs);
        glDeleteShader(fs);

        return shader;
    }
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

GLuint CyanRenderer::Toolkit::createTextureFromFile(const char* file, TextureType textureType, TexFilterType filterType)
{
    int w, h, numChannels;

    GLuint texId;
    GLenum texType, internalFormat, dataFormat, dataType, filter; 

    switch (textureType) 
    {
        case TextureType::TEX_2D:
            texType  = GL_TEXTURE_2D;
            break;
        case TextureType::TEX_CUBEMAP:
            texType  = GL_TEXTURE_CUBE_MAP;
            break;
        default:
            // TODO: Print out debug message
            break;
    }

    switch (filterType) 
    {
        case TexFilterType::LINEAR:
            filter = GL_LINEAR;
            break;
        default:
            // TODO: Print out debug message
            break;
    }

    void* pixels = stbi_load(file, &w, &h, &numChannels, 0);
    dataType = GL_UNSIGNED_BYTE;
    switch (numChannels)
    {
        case 3: 
            dataFormat = GL_RGB8;
            internalFormat = GL_RGB; 
            break;
        case 4:
            dataFormat = GL_RGBA8;
            internalFormat = GL_RGBA; 
            break;
        default:
            break;
    }

    glCreateTextures(texType, 1, &texId);
    glTextureStorage2D(texId, 1, dataFormat, w, h);
    glTextureSubImage2D(texId, 0, 0, 0, w, h, internalFormat, dataType, pixels);

    glBindTexture(texType, texId);
    glTexParameteri(texType, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(texType, GL_TEXTURE_MIN_FILTER, filter);
    glBindTexture(texType, 0);

    return texId;
}

GLuint CyanRenderer::Toolkit::createHDRTextureFromFile(const char* file, TextureType textureType, TexFilterType filterType)
{
    int w, h, numChannels;

    GLuint texId;
    GLenum texType, internalFormat, dataFormat, dataType, filter; 

    void* pixels = stbi_loadf(file, &w, &h, &numChannels, 0);
    dataType = GL_FLOAT;

    switch (textureType) 
    {
        case TextureType::TEX_2D:
            texType  = GL_TEXTURE_2D;
            break;
        case TextureType::TEX_CUBEMAP:
            texType  = GL_TEXTURE_CUBE_MAP;
            break;
        default:
            // TODO: Print out debug message
            break;
    }

    switch (filterType) 
    {
        case TexFilterType::LINEAR:
            filter  = GL_LINEAR;
            break;
        default:
            // TODO: Print out debug message
            break;
    }

    switch (numChannels)
    {
        case 3: 
            dataFormat = GL_RGB16F;
            internalFormat = GL_RGB; 
            break;
        case 4:
            dataFormat = GL_RGBA16F;
            internalFormat = GL_RGBA; 
            break;
        default:
            break;
    }

    glCreateTextures(texType, 1, &texId);
    glTextureStorage2D(texId, 1, dataFormat, w, h);
    glTextureSubImage2D(texId, 0, 0, 0, w, h, internalFormat, dataType, pixels);

    glBindTexture(texType, texId);
    glTexParameteri(texType, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(texType, GL_TEXTURE_MIN_FILTER, filter);
    glBindTexture(texType, 0);

    return texId;
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
glm::mat4 CyanRenderer::Toolkit::normalizeMeshScale(MeshGroup* mesh)
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

void CyanRenderer::Toolkit::loadScene(Scene& scene, const char* filename)
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
        // TODO: This should take into account of the window width and height;
        scene.mainCamera.projection = glm::perspective(glm::radians(scene.mainCamera.fov), 800.f / 600.f, scene.mainCamera.n, scene.mainCamera.f);
    }

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
}

VertexInfo CyanRenderer::Toolkit::loadSubMesh(aiMesh* assimpMesh, uint8_t* attribFlag)
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

MeshGroup* CyanRenderer::createMeshGroup(const char* meshName)
{
    MeshGroup* mesh = new MeshGroup();
    mRenderAssets.meshes.push_back(mesh);
    mesh->desc = meshName;
    mesh->normalizeXform = glm::mat4(1.f);
    return mesh;
}

MeshGroup* CyanRenderer::createCubeMesh(const char* meshName)
{
    MeshGroup* parentMesh = createMeshGroup(meshName);
    Mesh* mesh = new Mesh();
    mesh->setVertexBuffer(VertexBuffer::create(cubeVertices, sizeof(cubeVertices)));
    mesh->setNumVerts(sizeof(cubeVertices) / (sizeof(float) * 3));
    VertexBuffer* vb = mesh->getVertexBuffer();
    vb->pushVertexAttribute({{"Position"}, GL_FLOAT, 3, 0});
    mesh->initVertexAttributes();
    MeshManager::pushSubMesh(parentMesh, mesh);
    return parentMesh;
}

// TODO: Think about removing unused asset maybe?
MeshGroup* CyanRenderer::createMeshFromFile(const char* fileName)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        fileName,
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
    );

    MeshGroup* mesh = new MeshGroup();

    for (int i = 0; i < scene->mNumMeshes; i++) 
    {
        Mesh* subMesh = new Mesh();
        mesh->subMeshes.push_back(subMesh);

        subMesh->vertexInfo = Toolkit::loadSubMesh(scene->mMeshes[i], &subMesh->mAttribFlag);
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
    mesh->normalizeXform = Toolkit::normalizeMeshScale(mesh);
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

GLuint CyanRenderer::createCubemapFromTexture(ShaderBase* shader, GLuint envmap, MeshGroup* cubeMesh, int windowWidth, int windowHeight, int viewportWidth, int viewportHeight, bool hdr)
{
    GLuint fbo, rbo, cubemap;
    Camera camera = { };
    camera.position = glm::vec3(0.f);
    camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

    glCreateFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    glCreateRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    const int kNumFaces = 6;
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

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (int f = 0; f < 6; f++)
    {
        if (hdr)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F, viewportWidth, viewportHeight, 0, GL_RGB, GL_FLOAT, nullptr);
        else
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA8, viewportWidth, viewportHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    EnvmapShaderVars vars = { };
    glDisable(GL_DEPTH_TEST);

    // Since we are rendering to a framebuffer, we need to configure the viewport 
    // to prevent the texture being stretched to fit the framebuffer's dimension
    glViewport(0, 0, viewportWidth, viewportHeight);
    for (int f = 0; f < kNumFaces; f++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cubemap, 0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }

        // Update view matrix
        camera.lookAt = cameraTargets[f];
        camera.worldUp = worldUps[f];
        CameraManager::updateCamera(camera);
        vars.view = camera.view;
        vars.projection = camera.projection;
        vars.envmap = envmap; 
        shader->setShaderVariables(&vars);
        shader->prePass();
        drawMesh(*cubeMesh);
    }
    // Recover the viewport dimensions
    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  
    return cubemap;
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

void CyanRenderer::setupMSAA() {

}

void CyanRenderer::bloomPostProcess() {

}

void CyanRenderer::blitBackbuffer() {

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