#include <iostream>
#include <chrono>
#include "glew.h"
#include "glfw3.h"
#include "cyanRenderer.h"
#include "window.h"
#include "shader.h"
#include "scene.h"
#include "camera.h"
#include "mathUtils.h"

#define BLUR_ITERATION 5

struct Grid {
    uint32_t gridSize; 
    uint32_t numOfVerts;
    GLuint vbo;
    GLuint vao;
    float* verts;
    void initRenderParams();
    void generateVerts();
    void printGridVerts(); // Debugging purposes
};

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

void handleInput(Window& window, Camera& camera, float deltaTime) {
    double cursorX, cursorY;
    glfwGetCursorPos(window.mpWindow, &cursorX, &cursorY);
    if (window.keys[GLFW_KEY_W]) {
        camera.processKeyboard(GLFW_KEY_W, deltaTime);
    }
    if (window.keys[GLFW_KEY_S]) {
        camera.processKeyboard(GLFW_KEY_S, deltaTime);
    }
    if (window.keys[GLFW_KEY_A]) {
        camera.processKeyboard(GLFW_KEY_A, deltaTime);
    }
    if (window.keys[GLFW_KEY_D]) {
        camera.processKeyboard(GLFW_KEY_D, deltaTime);
    }
    if (window.keys[GLFW_MOUSE_BUTTON_MIDDLE]) {
        camera.processMouse(cursorX - window.lastX, cursorY - window.lastY, deltaTime);
        camera.eMode = ControlMode::orbit;
    }
    if (window.keys[GLFW_MOUSE_BUTTON_RIGHT]) {
        camera.processMouse(cursorX - window.lastX, cursorY - window.lastY, deltaTime);
        camera.eMode = ControlMode::free;
    }
    window.lastX = cursorX;
    window.lastY = cursorY;
}

// TODO: @camera orbit-mode controls
// TODO: @clean up control-flow with enabling MSAA
int main(int argc, char* argv[]) {
    std::cout << "Hello Cyan!" << std::endl;
    if (!glfwInit()) {
        std::cout << "glfw init failed" << std::endl;
    }

    Window mainWindow = {
        nullptr, 0, 0, 0, 0,
        {0}
    };

    windowManager.initWindow(mainWindow);
    glfwMakeContextCurrent(mainWindow.mpWindow);
    glewInit();

    // Initialization
    Camera mainCamera;
    CyanRenderer renderer;
    renderer.initRenderer();
    renderer.initShaders();
    Shader gridShader;
    Scene scene;
    sceneManager.loadSceneFromFile(scene, "scene/default_scene/scene_config.json");

    gridShader.init();
    gridShader.loadShaderSrc("shader/gridShader.vert", "shader/gridShader.frag");
    gridShader.generateShaderProgram();
    gridShader.bindShader();
    {
        std::vector<std::string> gridUniformNames;
        gridUniformNames.push_back("model");
        gridUniformNames.push_back("view");
        gridUniformNames.push_back("projection");
        gridShader.initUniformLoc(gridUniformNames);
    }

    Grid floorGrid = {
        20, 0, 0, 0, nullptr
    };

    floorGrid.generateVerts();
    floorGrid.initRenderParams();
    Transform gridXform = {
        glm::vec3(20.f, 20.f, 20.f),
        glm::vec3(0, 0, 0),
        glm::vec3(0, 0, 0.f)
    };
    glm::mat4 gridModelMat = MathUtils::transformToMat4(gridXform);
    glm::mat4 projection = glm::perspective(glm::radians(mainCamera.fov), 800.f / 600.f, mainCamera.near, mainCamera.far);
    glm::mat4 view;
    gridShader.setUniformMat4f("model", glm::value_ptr(gridModelMat));
    gridShader.setUniformMat4f("projection", glm::value_ptr(projection));
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    std::chrono::high_resolution_clock::time_point current, last = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> timeSpan; 

    while (!glfwWindowShouldClose(mainWindow.mpWindow)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //----- FPS -----------------
        current = std::chrono::high_resolution_clock::now();
        timeSpan = std::chrono::duration_cast<std::chrono::duration<float>>(current - last);
        last = current;
        //---------------------------

        //---- Input handling -------
        handleInput(mainWindow, scene.mainCamera, .06); // TODO: need to swap out the hard-coded value later
        //---------------------------

        //-- framebuffer operations --
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.defaultFBO);
        if (renderer.enableMSAA) {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.MSAAFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, renderer.MSAAColorBuffer, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, renderer.MSAADepthBuffer, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer.colorBuffer, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer.depthBuffer, 0);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        //----------------------------

        //----- Draw calls -----------
        //----- Default first pass ---
        renderer.drawScene(scene);

        gridShader.bindShader();
        gridShader.setUniformMat4f("view", glm::value_ptr(scene.mainCamera.view));
        glBindVertexArray(floorGrid.vao);
        glDrawArrays(GL_LINES, 0, floorGrid.numOfVerts);
        glUseProgram(0);
        glBindVertexArray(0);
        //-------------------------

        if (renderer.enableMSAA) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.MSAAFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer.intermFBO);
            glBlitFramebuffer(0, 0, 800, 600, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        } else {

        }

        //---- Render to hdr fbo--
        //---- Multi-render target---
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.hdrFBO);
        renderer.shaderPool[(int)ShadingMode::bloom].bindShader();
        GLuint renderTargert[2] = {
            GL_COLOR_ATTACHMENT0, 
            GL_COLOR_ATTACHMENT1
        };
        glDrawBuffers(2, renderTargert);
        //---------------------------
        glBindVertexArray(renderer.quadVAO);
        if (renderer.enableMSAA) {
            glBindTexture(GL_TEXTURE_2D, renderer.intermColorBuffer);
        } else {
            glBindTexture(GL_TEXTURE_2D, renderer.colorBuffer);
        }
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        //------------------------------------

        //---- Apply blur -----
        for (int itr = 0; itr < BLUR_ITERATION; itr++) {
            //---- Horizontal pass ----
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.pingPongFBO[itr * 2 % 2]);
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].bindShader();
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 1);
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 800.f);
            glBindVertexArray(renderer.quadVAO);
            if (itr == 0) {
                glBindTexture(GL_TEXTURE_2D, renderer.colorBuffer1);
            } else {
                glBindTexture(GL_TEXTURE_2D, renderer.pingPongColorBuffer[(itr * 2 - 1) % 2]);
            }
            glDrawArrays(GL_TRIANGLES, 0, 6);
            //--------------------- 

            //---- Vertical pass ----
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.pingPongFBO[(itr * 2 + 1) % 2]);
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].bindShader();
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].setUniform1i("horizontalPass", 0);
            renderer.shaderPool[(int)ShadingMode::gaussianBlur].setUniform1f("offset", 1.f / 600.f);
            glBindVertexArray(renderer.quadVAO);
            glBindTexture(GL_TEXTURE_2D, renderer.pingPongColorBuffer[itr * 2 % 2]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            //--------------------- 
        }

        //---- Blend final output with original hdrFramebuffer ----
        if (renderer.enableMSAA) {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.intermFBO);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.defaultFBO);
        }
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        renderer.shaderPool[(int)ShadingMode::bloomBlend].bindShader();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer.colorBuffer0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer.pingPongColorBuffer[1]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        //---------------------------------
        //--------------------- 

        //---- Render final output to default fbo--
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        renderer.quadShader.bindShader();
        glBindVertexArray(renderer.quadVAO);

        glActiveTexture(GL_TEXTURE0);
        if (renderer.enableMSAA) {
            glBindTexture(GL_TEXTURE_2D, renderer.intermColorBuffer);
        } else {
            glBindTexture(GL_TEXTURE_2D, renderer.colorBuffer);
        }

        glViewport(0, 0, 400, 300);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer.pingPongColorBuffer[1]);
        glViewport(400, 0, 400, 300);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glViewport(0, 0, 800, 600); // Need to reset the viewport state so that it doesn't affect offscreen MSAA rendering 
 
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        //----------------------------

        glfwSwapBuffers(mainWindow.mpWindow);
        glfwPollEvents();
    }
    return 0;
}