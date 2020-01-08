#include <iostream>
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "cyanRenderer.h"
#include "mathUtils.h"

#define BLUR_ITERATION 10

float quadVerts[24] = {
    -1.f, -1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f,

    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f
};

// TODO: @Refactor setting up framebuffers
void CyanRenderer::initRenderer() {
    enableMSAA = true;

    //---- Shader initialization ----
    quadShader.init();
    shaderPool[static_cast<int>(ShadingMode::grid)].init();
    shaderPool[static_cast<int>(ShadingMode::blinnPhong)].init(); 
    shaderPool[static_cast<int>(ShadingMode::cubemap)].init(); 
    shaderPool[static_cast<int>(ShadingMode::flat)].init();
    shaderPool[static_cast<int>(ShadingMode::bloom)].init();
    shaderPool[static_cast<int>(ShadingMode::bloomBlend)].init();
    shaderPool[static_cast<int>(ShadingMode::gaussianBlur)].init();
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

    activeShaderIdx = (int)ShadingMode::grid;
    shaderPool[activeShaderIdx].loadShaderSrc("shader/gridShader.vert", "shader/gridShader.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("model");
        uniformNames.push_back("view");
        uniformNames.push_back("projection");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }


    activeShaderIdx = (int)ShadingMode::blinnPhong;
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

    activeShaderIdx = static_cast<int>(ShadingMode::flat);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/flatShader.vert", "shader/flatShader.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("model");
        uniformNames.push_back("view");
        uniformNames.push_back("projection");
        uniformNames.push_back("color");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }
    
    activeShaderIdx = static_cast<int>(ShadingMode::bloom);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/bloomShader.vert", "shader/bloomShader.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();

    activeShaderIdx = static_cast<int>(ShadingMode::bloomBlend);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/bloomBlend.vert", "shader/bloomBlend.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("colorBuffer");
        uniformNames.push_back("brightColorBuffer");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }
    glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["colorBuffer"], 0);
    glUniform1i(shaderPool[activeShaderIdx].uniformLocMap["brightColorBuffer"], 1);

    activeShaderIdx = static_cast<int>(ShadingMode::gaussianBlur);
    shaderPool[activeShaderIdx].loadShaderSrc("shader/gaussianBlur.vert", "shader/gaussianBlur.frag");
    shaderPool[activeShaderIdx].generateShaderProgram();
    shaderPool[activeShaderIdx].bindShader();
    {
        std::vector<std::string> uniformNames;
        uniformNames.push_back("horizontalPass");
        uniformNames.push_back("offset");
        shaderPool[activeShaderIdx].initUniformLoc(uniformNames);
    }

    // TODO: need to clean this up
    activeShaderIdx = static_cast<int>(ShadingMode::blinnPhong);

    glUseProgram(0);
}

void CyanRenderer::prepareBlinnPhongShader(Scene& scene, MeshInstance& instance) {
    Mesh mesh = scene.meshList[instance.meshID];
    Transform xform = scene.xformList[instance.instanceID];
    // TODO: Handle rotation
    glm::mat4 model(1.f);
    model = glm::translate(model, xform.translation);
    model = glm::scale(model, xform.scale);
    shaderPool[(int)ShadingMode::blinnPhong].setUniformMat4f("model", glm::value_ptr(model));
    shaderPool[(int)ShadingMode::blinnPhong].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    shaderPool[(int)ShadingMode::blinnPhong].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));

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
}

void CyanRenderer::prepareGridShader(Scene& scene) {
    Transform gridXform = {
        glm::vec3(20.f, 20.f, 20.f),
        glm::vec3(0, 0, 0),
        glm::vec3(0, 0, 0.f)
    };
    glm::mat4 gridModelMat = MathUtils::transformToMat4(gridXform);
    shaderPool[(int)ShadingMode::grid].setUniformMat4f("model", glm::value_ptr(gridModelMat));
    shaderPool[(int)ShadingMode::grid].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    shaderPool[(int)ShadingMode::grid].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));
}

void CyanRenderer::prepareFlatShader(Scene& scene, MeshInstance& instance) {
    Mesh mesh = scene.meshList[instance.meshID];
    Transform xform = scene.xformList[instance.instanceID];
    // TODO: Handle rotation
    glm::mat4 model(1.f);
    model = glm::translate(model, xform.translation);
    model = glm::scale(model, xform.scale);
    shaderPool[(int)ShadingMode::flat].setUniformMat4f("model", glm::value_ptr(model));
    shaderPool[(int)ShadingMode::flat].setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
    shaderPool[(int)ShadingMode::flat].setUniformMat4f("projection", glm::value_ptr(scene.mainCamera.projection));

    // Set color
    glm::vec3 color(1.0, 0.84, 0.67);
    color *= 1.2;
    shaderPool[(int)ShadingMode::flat].setUniformVec3("color", glm::value_ptr(color));
}

// TODO: Image-based lighting
// TODO: Refactor dealing with uniform names
// TODO: Imgui
// TODO: Mouse Picking
// TODO: Transitioning to forward+ or tile-based deferred shading 
void CyanRenderer::drawInstance(Scene& scene, MeshInstance& instance) {
    Mesh mesh = scene.meshList[instance.meshID];
    //---
    // TODO: Probably will get rid of this in the future
    // TODO: Configure shader input based on different types of shader
    activeShaderIdx = scene.meshList[instance.meshID].shaderIdx;
    //---

    glBindVertexArray(mesh.vao);
    shaderPool[activeShaderIdx].bindShader();

    switch (activeShaderIdx)
    {
        case 3: {
            prepareBlinnPhongShader(scene, instance);
            break;
        }

        case 5: {
            prepareFlatShader(scene, instance);
            break;
        }
    
        default:
            break;
    }

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
    //---- Draw skybox -----
    drawSkybox(scene);

    shaderPool[(int)ShadingMode::grid].bindShader();
    prepareGridShader(scene);
    drawGrid();
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

void CyanRenderer::setupMSAA() {
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAAColorBuffer, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, MSAADepthBuffer, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

void CyanRenderer::bloomPostProcess() {
    //---- Render to hdr fbo--
    //---- Multi-render target---
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    shaderPool[(int)ShadingMode::bloom].bindShader();
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
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    //------------------------------------

    //---- Apply blur -----
    for (int itr = 0; itr < BLUR_ITERATION; itr++) {
        //---- Horizontal pass ----
        glBindFramebuffer(GL_FRAMEBUFFER, pingPongFBO[itr * 2 % 2]);
        shaderPool[(int)ShadingMode::gaussianBlur].bindShader();
        shaderPool[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 1);
        shaderPool[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 800.f);
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
        shaderPool[(int)ShadingMode::gaussianBlur].bindShader();
        shaderPool[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 0);
        shaderPool[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 600.f);
        glBindVertexArray(quadVAO);
        glBindTexture(GL_TEXTURE_2D, pingPongColorBuffer[itr * 2 % 2]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        //--------------------- 
    }

    //---- Blend final output with original hdrFramebuffer ----
    if (enableMSAA) {
        glBindFramebuffer(GL_FRAMEBUFFER, intermFBO);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    }
    
    shaderPool[(int)ShadingMode::bloomBlend].bindShader();
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
}

void CyanRenderer::blitBackbuffer() {
    //---- Render final output to default fbo--
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    quadShader.bindShader();
    glBindVertexArray(quadVAO);

    glActiveTexture(GL_TEXTURE0);
    if (enableMSAA) {
        glBindTexture(GL_TEXTURE_2D, intermColorBuffer);
    } else {
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
    }

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

void Grid::generateVerts() {
    int lineIdx = 0;
    int numOfLines = gridSize + 1;
    float cellSize = 1.f / gridSize;
    float startX = -.5f;
    float startZ = -startX;
    // 6 extra verts for drawing three axis
    this->numOfVerts = 4 * (gridSize + 1) + 6;
    verts = new float[numOfVerts * 3 * 2]; 
    int vertIdx = lineIdx * 2 * 2;
    for (; lineIdx < numOfLines; lineIdx++) {
        // vertical line
        verts[vertIdx * 6    ] = startX + lineIdx * cellSize;    // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = startZ;                         // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        verts[vertIdx * 6    ] =  startX + lineIdx * cellSize;   // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = -startZ;                        // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        // horizontal li6e
        verts[vertIdx * 6    ] = startX;                         // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] = startZ- lineIdx * cellSize;     // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;

        verts[vertIdx * 6    ] = -startX;                        // x
        verts[vertIdx * 6 + 1] = 0.f;                            // y
        verts[vertIdx * 6 + 2] =  startZ - lineIdx * cellSize;   // z
        verts[vertIdx * 6 + 3] = .5f;                       // r
        verts[vertIdx * 6 + 4] = .5f;                       // g
        verts[vertIdx * 6 + 5] = .5f;                       // b
        vertIdx++;
    }

    // verts for three axis
    verts[vertIdx * 6    ] = -1.f;                      // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 1.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 1.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 1.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = -1.f;                      // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 1.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 1.f;                       // y
    verts[vertIdx * 6 + 2] = 0.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 1.f;                       // g
    verts[vertIdx * 6 + 5] = 0.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = -1.f;                      // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 1.f;                       // b
    vertIdx++;
    verts[vertIdx * 6    ] = 0.f;                       // x
    verts[vertIdx * 6 + 1] = 0.f;                       // y
    verts[vertIdx * 6 + 2] = 1.f;                       // z
    verts[vertIdx * 6 + 3] = 0.f;                       // r
    verts[vertIdx * 6 + 4] = 0.f;                       // g
    verts[vertIdx * 6 + 5] = 1.f;                       // b
    vertIdx++;
}