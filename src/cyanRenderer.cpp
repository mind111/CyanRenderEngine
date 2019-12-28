#include <iostream>
#include "cyanRenderer.h"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

float quadVerts[24] = {
    -1.f, -1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f,

    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f
};

void CyanRenderer::initRenderer() {
    enableMSAA = true;

    //---- Shader initialization ----
    quadShader.init();
    shaderPool[static_cast<int>(ShadingMode::blinnPhong)].init(); 
    shaderPool[static_cast<int>(ShadingMode::cubemap)].init(); 
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        std::cout << "incomplete framebuffer!" << std::endl;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
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
    //---------------------------------
} 

void CyanRenderer::initShaders() {
    // TODO: set up all the shaders
    // for (auto shader : shaderPool) {
    //     shader.loadShaderSrc();
    //     shader.generateShaderProgram();
    //     shader.initUniformLoc();
    // }

    // TODO: hard-code shader name and according shader src for now
    quadShader.loadShaderSrc("shader/quadShader.vert", "shader/quadShader.frag");
    quadShader.generateShaderProgram();
    quadShader.bindShader();

    activeShaderIdx = static_cast<int>(ShadingMode::blinnPhong);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/blinnPhong.vert", "shader/blinnPhong.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("model");
        uniformNames.push_back("view");
        uniformNames.push_back("projection");
        uniformNames.push_back("dLight.baseLight.color");
        uniformNames.push_back("dLight.direction");
        uniformNames.push_back("normalSampler");
        uniformNames.push_back("numDiffuseMaps");
        uniformNames.push_back("numSpecularMaps");
        uniformNames.push_back("hasNormalMap");
        uniformNames.push_back("diffuseSamplers[0]");
        uniformNames.push_back("diffuseSamplers[1]");
        uniformNames.push_back("diffuseSamplers[2]");
        uniformNames.push_back("diffuseSamplers[3]");
        uniformNames.push_back("specularSampler");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }

    activeShaderIdx = static_cast<int>(ShadingMode::cubemap);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/skyboxShader.vert", "shader/skyboxShader.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("model");
        uniformNames.push_back("view");
        uniformNames.push_back("projection");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }

    // TODO: need to clean this up
    activeShaderIdx = static_cast<int>(ShadingMode::blinnPhong);

    glUseProgram(0);
}

// TODO: Phong/blinn-phong shader
// TODO: Cubemap
// TODO: Image-based lighting
// TODO: Refactor dealing with uniform names
// TODO: Imgui
// TODO: Mouse Picking

// TODO: Transitioning to forward+ or tile-based deferred shading 
void CyanRenderer::drawInstance(Scene& scene, MeshInstance& instance) {
    Mesh mesh = scene.meshList[instance.meshID];
    Transform xform = scene.xformList[instance.instanceID];
    glBindVertexArray(mesh.vao);
    shaderPool[activeShaderIdx].bindShader();    

    //---- Configure transformation uniforms -----
    // TODO: Handle rotation
    glm::mat4 model(1.f);
    model = glm::translate(model, xform.translation);
    model = glm::scale(model, xform.scale);
    shaderPool[activeShaderIdx].setUniformMat4f("model", glm::value_ptr(model));
    shaderPool[activeShaderIdx].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    shaderPool[activeShaderIdx].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));
    //-----------------------------

    //---- Configure lighting uniforms ----- 
    //---- Lighting calculation is done in view space
    DirectionLight dLight;    
    dLight.color = glm::vec3(1.0f, 0.84f, 0.67f);
    dLight.intensity = .8f;
    dLight.direction = glm::vec3(-1.f, -.5f, -1.f);
    shaderPool[activeShaderIdx].setUniformVec3("dLight.baseLight.color", glm::value_ptr(dLight.color));
    shaderPool[activeShaderIdx].setUniformVec3("dLight.direction", glm::value_ptr(dLight.direction));
    //--------------------------------------

    //---- Configure texture uniforms -----
    int numDiffuseMap = 0;
    glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["numDiffuseMaps"], mesh.diffuseMapTable.size());
    for (auto itr = mesh.diffuseMapTable.begin(); itr != mesh.diffuseMapTable.end(); itr++) {
        std::string samplerName("diffuseSamplers[" + std::to_string(numDiffuseMap) + "]");
        int samplerLoc = shaderPool[activeShaderIdx].uniformLocMap[samplerName];
        glUniform1i(samplerLoc, numDiffuseMap);
        glActiveTexture(GL_TEXTURE0 + numDiffuseMap++);
        glBindTexture(GL_TEXTURE_2D, scene.textureList[itr->second].textureObj);
    }
    for (numDiffuseMap; numDiffuseMap < 4; numDiffuseMap++) {
        std::string samplerName("diffuseSamplers[" + std::to_string(numDiffuseMap) + "]");
        int samplerLoc = shaderPool[activeShaderIdx].uniformLocMap[samplerName];
        glUniform1i(samplerLoc, numDiffuseMap);
    }

    // TODO: progress marker here
    if (mesh.specularMapTable.size() > 0) {
        glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["numSpecularMaps"], 1);
        glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["specularSampler"], numDiffuseMap);
        glActiveTexture(GL_TEXTURE0 + numDiffuseMap++);
        glBindTexture(GL_TEXTURE_2D, scene.textureList[mesh.specularMapTable.begin()->second].textureObj);
    }
    if (mesh.normalMapID != -1) {
        glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["hasNormalMap"], 1);
        int samplerLoc = shaderPool[activeShaderIdx].uniformLocMap["normalSampler"];
        glUniform1i(samplerLoc, numDiffuseMap);
        glActiveTexture(GL_TEXTURE0 + numDiffuseMap);
        glBindTexture(GL_TEXTURE_2D, scene.textureList[mesh.normalMapID].textureObj);
    } else {
        glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["hasNormalMap"], 0);
    }
    //--------------------------------------

    //---- Draw call --------------
    glDrawArrays(GL_TRIANGLES, 0, mesh.numVerts);
    //-----------------------------

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void CyanRenderer::drawScene(Scene& scene) {
    // Update view per frame
    scene.mainCamera.updateView();

    for (auto instance : scene.instanceList) {
        drawInstance(scene, instance);
    }
    //---- Draw skybox ----
    drawSkybox(scene);
}

// TODO: Cancel out the translation part of view matrix
void CyanRenderer::drawSkybox(Scene& scene) {
    shaderPool[(int)ShadingMode::cubemap].bindShader();
    glm::mat4 cubeXform(1.f);
    cubeXform = glm::scale(cubeXform, glm::vec3(100.f));
    shaderPool[(int)ShadingMode::cubemap].setUniformMat4f("model", glm::value_ptr(cubeXform));
    shaderPool[(int)ShadingMode::cubemap].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));
    shaderPool[(int)ShadingMode::cubemap].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    glBindVertexArray(scene.skybox.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, scene.skybox.cubmapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}