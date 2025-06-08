#include <GL/glew.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm> // Required for std::sort

#include "shader.hpp" // Assuming you have this for LoadShaders
#include "Model.h"    // Your Model class header
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // For texture loading

using namespace glm;
using namespace std;

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void printControls();

// --- Constants ---
const unsigned int SCR_WIDTH = 900;
const unsigned int SCR_HEIGHT = 1200;
const int MAX_POINT_LIGHTS_SUPPORTED = 20; // Ensure this matches or exceeds number of lights used
const float SUN_ANIMATION_SPEED = 0.02f;
const float SUN_MOVEMENT_RANGE_X = 0.8f;
const float SUN_BASE_Y_DIRECTION = -0.7f;
const float SUN_BASE_Z_DIRECTION = -0.5f;

// Shadow Mapping Constants
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

// --- Drone Camera ---
struct Drone {
    glm::vec3 position = glm::vec3(0.0f, 1.7f, 10.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 5.0f;

    void updateFront() {
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(direction);
    }

    void rotateYaw(float offset) {
        yaw += offset;
        updateFront();
    }

    void rotatePitch(float offset) {
        pitch += offset;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateFront();
    }

    void reset() {
        position = glm::vec3(0.0f, 1.7f, 10.0f);
        front = glm::vec3(0.0f, 0.0f, -1.0f);
        up = glm::vec3(0.0f, 1.0f, 0.0f);
        yaw = -90.0f;
        pitch = 0.0f;
        updateFront();
    }

    void printStatus() {
        std::cout << "\n--- Drone Status ---" << std::endl;
        std::cout << "Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
        std::cout << "Looking: (" << front.x << ", " << front.y << ", " << front.z << ")" << std::endl;
        std::cout << "Yaw: " << yaw << ", Pitch: " << pitch << std::endl;
        std::cout << "-------------------" << std::endl;
    }
};
Drone drone;

// --- Keyboard State ---
struct KeyboardState {
    bool forward = false;    // W
    bool backward = false;   // S
    bool left = false;       // A
    bool right = false;      // D
    bool up = false;         // Space
    bool down = false;       // Left Shift
    bool rotateLeft = false; // Q
    bool rotateRight = false;// E
    bool lookUp = false;     // I
    bool lookDown = false;   // K
} keys;

// --- Timing ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- Field of View ---
float fov = 45.0f;

// --- Model Information ---
struct ModelInfo {
    Model* model;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    bool isTransparent;
    bool isGlass;

    ModelInfo(Model* m = nullptr,
              glm::vec3 pos = glm::vec3(0.0f),
              glm::vec3 rot = glm::vec3(0.0f),
              glm::vec3 scl = glm::vec3(1.0f),
              bool transparent = false,
              bool glass = false)
        : model(m), position(pos), rotation(rot), scale(scl),
          isTransparent(transparent), isGlass(glass) {}
};

// --- Transparent Object Sorting ---
struct TransparentObject {
    const ModelInfo* modelInfo;
    std::string name;
    float distance;

    TransparentObject(const ModelInfo* info, const std::string& n, float dist)
        : modelInfo(info), name(n), distance(dist) {}

    bool operator<(const TransparentObject& other) const {
        return distance > other.distance; // Sort far to near
    }
};

// Global variables for Shadow Mapping
GLuint depthMapFBO;
GLuint depthMapTexture;
GLuint depthShaderProgram_global;

// --- Function to Render Transparent Objects ---
void renderTransparentObjects(
    GLuint shaderProgram,
    const std::map<std::string, ModelInfo>& models,
    const glm::vec3& cameraPos,
    GLint isGlassLocation,
    bool glassOnly = false) {

    std::vector<TransparentObject> transparentObjects;
    for (const auto& pair : models) {
        const ModelInfo& modelInfo = pair.second;
        if (!modelInfo.isTransparent) continue;
        if (glassOnly && !modelInfo.isGlass) continue;
        if (!glassOnly && modelInfo.isGlass) continue;
        if (!modelInfo.model) continue;
        float distance = glm::length(cameraPos - modelInfo.position);
        transparentObjects.emplace_back(&modelInfo, pair.first, distance);
    }
    std::sort(transparentObjects.begin(), transparentObjects.end());

    for (const auto& obj : transparentObjects) {
        const ModelInfo& modelInfo = *obj.modelInfo;
        if (isGlassLocation != -1) {
            glUniform1i(isGlassLocation, modelInfo.isGlass ? 1 : 0);
        }
        float shininess = modelInfo.isGlass ? 96.0f : 32.0f;
        glUniform1f(glGetUniformLocation(shaderProgram, "material.shininess"), shininess);

        // Example material properties for transparent objects (can be refined)
        GLint matAmbientLoc_trans = glGetUniformLocation(shaderProgram, "material.ambient");
        GLint matSpecularLoc_trans = glGetUniformLocation(shaderProgram, "material.specular");

        if (modelInfo.isGlass) {
            if(matAmbientLoc_trans != -1) glUniform3f(matAmbientLoc_trans, 0.05f, 0.05f, 0.08f); // Slightly blueish ambient for glass
            if(matSpecularLoc_trans != -1) glUniform3f(matSpecularLoc_trans, 0.7f, 0.7f, 0.8f);  // Stronger specular for glass
        } else { // Plants
            if(matAmbientLoc_trans != -1) glUniform3f(matAmbientLoc_trans, 0.02f, 0.03f, 0.01f); // Dim green ambient for plants
            if(matSpecularLoc_trans != -1) glUniform3f(matSpecularLoc_trans, 0.05f, 0.05f, 0.05f); // Low specular for plants
        }


        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, modelInfo.position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(modelInfo.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(modelInfo.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(modelInfo.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::scale(modelMatrix, modelInfo.scale);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        modelInfo.model->Draw(shaderProgram);
    }
}

// --- Print Controls & Callbacks ---
void printControls() {
    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "W/A/S/D: Move forward/left/backward/right" << std::endl;
    std::cout << "Space/Left Shift: Move up/down" << std::endl;
    std::cout << "Q/E: Rotate (yaw) left/right" << std::endl;
    std::cout << "I/K: Look (pitch) up/down" << std::endl;
    std::cout << "R: Reset drone to initial position" << std::endl;
    std::cout << "P: Print drone status" << std::endl;
    std::cout << "F1: Show controls" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "=================" << std::endl;
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
void processInput(GLFWwindow *window) {
    float currentSpeed = drone.speed * deltaTime;
    float rotationSpeed = 50.0f * deltaTime;

    if (keys.forward) drone.position += drone.front * currentSpeed;
    if (keys.backward) drone.position -= drone.front * currentSpeed;
    if (keys.left) drone.position -= glm::normalize(glm::cross(drone.front, drone.up)) * currentSpeed;
    if (keys.right) drone.position += glm::normalize(glm::cross(drone.front, drone.up)) * currentSpeed;
    if (keys.up) drone.position += drone.up * currentSpeed;
    if (keys.down) drone.position -= drone.up * currentSpeed;

    if (keys.rotateLeft) drone.rotateYaw(-rotationSpeed);
    if (keys.rotateRight) drone.rotateYaw(rotationSpeed);
    if (keys.lookUp) drone.rotatePitch(rotationSpeed);
    if (keys.lookDown) drone.rotatePitch(-rotationSpeed);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_W: keys.forward = true; break;
            case GLFW_KEY_S: keys.backward = true; break;
            case GLFW_KEY_A: keys.left = true; break;
            case GLFW_KEY_D: keys.right = true; break;
            case GLFW_KEY_SPACE: keys.up = true; break;
            case GLFW_KEY_LEFT_SHIFT: keys.down = true; break;
            case GLFW_KEY_Q: keys.rotateLeft = true; break;
            case GLFW_KEY_E: keys.rotateRight = true; break;
            case GLFW_KEY_I: keys.lookUp = true; break;
            case GLFW_KEY_K: keys.lookDown = true; break;
            case GLFW_KEY_R: drone.reset(); std::cout << "Drone camera reset." << std::endl; break;
            case GLFW_KEY_P: drone.printStatus(); break;
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); break;
            case GLFW_KEY_F1: printControls(); break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (key) {
            case GLFW_KEY_W: keys.forward = false; break;
            case GLFW_KEY_S: keys.backward = false; break;
            case GLFW_KEY_A: keys.left = false; break;
            case GLFW_KEY_D: keys.right = false; break;
            case GLFW_KEY_SPACE: keys.up = false; break;
            case GLFW_KEY_LEFT_SHIFT: keys.down = false; break;
            case GLFW_KEY_Q: keys.rotateLeft = false; break;
            case GLFW_KEY_E: keys.rotateRight = false; break;
            case GLFW_KEY_I: keys.lookUp = false; break;
            case GLFW_KEY_K: keys.lookDown = false; break;
        }
    }
}

// --- Main Function ---
int main() {
    std::cout << "=== IT Kiosk Renderer ===" << std::endl;
    const glm::vec3 commonPointLightDiffuseStrength = glm::vec3(0.05f);
    const glm::vec3 commonPointLightSpecularStrength = glm::vec3(0.1f);
    const glm::vec3 commonPointLightAmbientStrength = glm::vec3(0.0002f);

    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "IT Kiosk - Shadows", NULL, NULL);
    if (window == NULL) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "Failed to initialize GLEW" << std::endl; glfwTerminate(); return -1; }
    std::cout << "Using GLEW " << glewGetString(GLEW_VERSION) << std::endl;

    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Depth Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "Loading shaders..." << std::endl;
    GLuint shaderProgram = LoadShaders("vertexShader.glsl", "fragmentShader.glsl");
    depthShaderProgram_global = LoadShaders("depth_vertex.glsl", "depth_fragment.glsl");
    if (shaderProgram == 0 || depthShaderProgram_global == 0) {std::cerr << "ERROR: Failed to load shaders!" << std::endl; glfwTerminate(); return -1; }
    std::cout << "✓ Shaders loaded successfully!" << std::endl;

    std::map<std::string, ModelInfo> models;
    try {
        const std::vector<std::string> modelNames = {
            "BackWall", "Barstools", "BarTables", "BlueCouches", "BrownChairs", "CharcoalChairs", "CircleSofas",
            "CoffeeTables", "Cubicles", "Dividers", "EntranceWall", "GreyChairs", "ITLabsLeft", "ITLabsRight", "Kiosk",
            "LabWallsLeft", "LabWallsRight", "LabWindowFrames", "MainFloor", "MiniCoffeeTable", "Railings",
            "RoofFraming", "Underflooring", "WallDecorLeft", "WallDecorRight", "CoffeeMachine", "CashRegister", "OtherLights", "Lights", "Cans", "PopcornMachine"
        };
        for (const auto& name : modelNames) {
            std::string modelPath = "models/" + name + ".obj";
            Model* loadedModel = new Model(modelPath);
            models[name] = ModelInfo(loadedModel);
            std::cout << "✓ " << name << " loaded." << std::endl;
        }
        Model* plantsModel = new Model("models/Plants.obj");
        models["Plants"] = ModelInfo(plantsModel, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), true, false);
        std::cout << "✓ Plants loaded." << std::endl;
        Model* windowsModel = new Model("models/AllGlass.obj");
        models["Windows"] = ModelInfo(windowsModel, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), true, true);
        std::cout << "✓ Windows loaded." << std::endl;
        Model* glassPanelsModel = new Model("models/GlassPanels.obj");
        models["GlassPanels"] = ModelInfo(glassPanelsModel, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), true, true);
        std::cout << "✓ Glass Panels loaded." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading models: " << e.what() << std::endl;
    }
    std::cout << "All models processed." << std::endl;
    printControls();

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLint isGlassLocation = glGetUniformLocation(shaderProgram, "isGlass");
    GLint matShininessLoc = glGetUniformLocation(shaderProgram, "material.shininess");
    GLint matAmbientLoc = glGetUniformLocation(shaderProgram, "material.ambient");
    GLint matSpecularLoc = glGetUniformLocation(shaderProgram, "material.specular");
    GLint texDiffuseLoc = glGetUniformLocation(shaderProgram, "material.texture_diffuse1");
    GLint texSpecularLoc = glGetUniformLocation(shaderProgram, "material.texture_specular1");
    GLint lightSpaceMatrixLoc_main = glGetUniformLocation(shaderProgram, "lightSpaceMatrix");
    GLint shadowMapLoc_main = glGetUniformLocation(shaderProgram, "shadowMap");

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window); // This updates drone.position and drone.front

        // --- 1. DEPTH PASS ---
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        float near_plane_light = 1.0f, far_plane_light = 150.0f;
        float orthoSize = 70.0f; // Adjusted to better encompass a larger scene

        float timeValue_sun = currentFrame; // Use currentFrame for consistency
        float sunDirectionX_anim = sin(timeValue_sun * SUN_ANIMATION_SPEED) * SUN_MOVEMENT_RANGE_X;
        glm::vec3 currentAnimatedSunDirection = glm::normalize(glm::vec3(sunDirectionX_anim, SUN_BASE_Y_DIRECTION, SUN_BASE_Z_DIRECTION));
        
        glm::vec3 sceneCenter = glm::vec3(0.0f, 10.0f, -10.0f); // Estimate of your scene's rough center for the light to look at
        glm::vec3 lightPos = sceneCenter - currentAnimatedSunDirection * 60.0f; // Position light source "behind" scene center

        lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane_light, far_plane_light);
        lightView = glm::lookAt(lightPos, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        lightSpaceMatrix = lightProjection * lightView;

        glUseProgram(depthShaderProgram_global);
        glUniformMatrix4fv(glGetUniformLocation(depthShaderProgram_global, "lightSpaceMatrix"), 1, GL_FALSE, value_ptr(lightSpaceMatrix));

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT); // Only clear depth
            // Optional: Front face culling for peter panning
            // glEnable(GL_CULL_FACE);
            // glCullFace(GL_FRONT);

            for (const auto& pair : models) {
                const ModelInfo& modelInfo = pair.second;
                if (modelInfo.isTransparent || !modelInfo.model) continue;

                glm::mat4 modelMatrix_depth = glm::mat4(1.0f);
                modelMatrix_depth = glm::translate(modelMatrix_depth, modelInfo.position);
                modelMatrix_depth = glm::rotate(modelMatrix_depth, glm::radians(modelInfo.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                modelMatrix_depth = glm::rotate(modelMatrix_depth, glm::radians(modelInfo.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                modelMatrix_depth = glm::rotate(modelMatrix_depth, glm::radians(modelInfo.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
                modelMatrix_depth = glm::scale(modelMatrix_depth, modelInfo.scale);
                glUniformMatrix4fv(glGetUniformLocation(depthShaderProgram_global, "model"), 1, GL_FALSE, value_ptr(modelMatrix_depth));
                modelInfo.model->Draw(depthShaderProgram_global);
            }
            // if (glIsEnabled(GL_CULL_FACE)) { // Reset culling if it was enabled
            //     glCullFace(GL_BACK);
            //     glDisable(GL_CULL_FACE);
            // }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        // --- END DEPTH PASS ---


        // --- 2. MAIN RENDER PASS ---
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 view = glm::lookAt(drone.position, drone.position + drone.front, drone.up); // Uses updated drone state
        
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, glm::value_ptr(drone.position));

        if (lightSpaceMatrixLoc_main != -1) {
            glUniformMatrix4fv(lightSpaceMatrixLoc_main, 1, GL_FALSE, value_ptr(lightSpaceMatrix));
        }
        if (shadowMapLoc_main != -1) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, depthMapTexture);
            glUniform1i(shadowMapLoc_main, 3);
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "dirLights[0].direction"), 1, value_ptr(currentAnimatedSunDirection));
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLights[0].ambient"), 0.001f, 0.001f, 0.001f);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLights[0].diffuse"), 0.45f, 0.3f, 0.15f);
        glUniform3f(glGetUniformLocation(shaderProgram, "dirLights[0].specular"), 0.4f, 0.35f, 0.25f);
        glUniform1i(glGetUniformLocation(shaderProgram, "dirLights[0].enabled"), 1);
        glUniform1i(glGetUniformLocation(shaderProgram, "numDirLights"), 1);

        int activePointLights = 0;
        // Light 1
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 0.154029f, -21.925095f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f); // Attenuation for ~160 units
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 2 (Original pointLights[1] - Transformed from Blender Light.002)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -30.696480f, -21.925095f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 3 (Transformed from Blender Light.003)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -65.954384f, -21.925095f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f); // Adjusted attenuation for potentially larger distance
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 4 (Transformed from Blender Light.004)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -1.367252f, 17.311728f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 5 (Transformed from Blender Light.005)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -32.221157f, 17.290623f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 6 (Transformed from Blender Light.006)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -67.484856f, 17.297579f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.007f); // Further distance
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0002f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 7 (Transformed from Blender Light.007)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 29.528439f, 17.187288f, -22.325785f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 8 (Transformed from Blender Cube.010)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 0.403565f, 16.272787f, -23.359404f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 9 (Transformed from Blender Cube.013)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -30.437645f, 16.273632f, -23.359404f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 10 (Transformed from Blender Cube.014)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -1.605055f, 16.320671f, -23.359404f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 11 (Transformed from Blender Cube.017)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 31.253157f, 16.073551f, -23.359404f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }

        // Light 12 (Blender Light.008)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 73.030205f, 29.086636f, 0.262677f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.007f); 
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0002f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 13 (Blender Light.009)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 73.030205f, 37.828785f, 0.262677f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.007f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0002f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 14 (Blender Light.011)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 73.030205f, 5.471813f, 0.262677f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 15 (Blender Light.014)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -97.400917f, 19.527908f, -29.851522f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.007f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0002f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 16 (Blender Light.015)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -66.798378f, 6.912896f, -28.229601f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.014f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0007f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 17 (Blender Light.016)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -44.519119f, -1.154248f, 16.305470f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 18 (Blender Light.017)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), 31.984005f, -1.154248f, 16.305470f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 19 (Blender Light.018)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -6.326900f, -1.154248f, 16.305470f);
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }
        // Light 20 (Blender Light.019 - Note: This was a duplicate position in your list, I used a slightly different Z for uniqueness)
        if (activePointLights < MAX_POINT_LIGHTS_SUPPORTED) {
            glUniform3f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].position").c_str()), -44.519119f, -1.154248f, 15.305470f); // Different Z
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].ambient").c_str()), 1, value_ptr(commonPointLightAmbientStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].diffuse").c_str()), 1, value_ptr(commonPointLightDiffuseStrength));
            glUniform3fv(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].specular").c_str()), 1, value_ptr(commonPointLightSpecularStrength));
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].linear").c_str()), 0.022f);
            glUniform1f(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].quadratic").c_str()), 0.0019f);
            glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(activePointLights) + "].enabled").c_str()), 1);
            activePointLights++;
        }

        glUniform1i(glGetUniformLocation(shaderProgram, "numPointLights"), activePointLights);
        for (int i = activePointLights; i < MAX_POINT_LIGHTS_SUPPORTED; ++i) {
             glUniform1i(glGetUniformLocation(shaderProgram, ("pointLights[" + std::to_string(i) + "].enabled").c_str()), 0);
        }
        // End Point Light Setup

        if (texDiffuseLoc != -1) glUniform1i(texDiffuseLoc, 0);
        if (texSpecularLoc != -1) glUniform1i(texSpecularLoc, 1);

        // --- Render Opaque Objects (Main Pass) ---
        glDepthMask(GL_TRUE);
        for (const auto& pair : models) {
            const ModelInfo& modelInfo = pair.second;
            if (modelInfo.isTransparent || !modelInfo.model) continue;
            if (isGlassLocation != -1) glUniform1i(isGlassLocation, 0);

            float shininess = 32.0f;
            if (pair.first.find("Table") != std::string::npos ||
                pair.first.find("Chair") != std::string::npos ||
                pair.first == "Dividers" || pair.first == "Railings" ||
                pair.first == "WallDecor") {
                shininess = 16.0f;
            }
            if (matShininessLoc != -1) glUniform1f(matShininessLoc, shininess);

            glm::mat4 modelMatrix_main = glm::mat4(1.0f);
            modelMatrix_main = glm::translate(modelMatrix_main, modelInfo.position);
            modelMatrix_main = glm::rotate(modelMatrix_main, glm::radians(modelInfo.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix_main = glm::rotate(modelMatrix_main, glm::radians(modelInfo.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix_main = glm::rotate(modelMatrix_main, glm::radians(modelInfo.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            modelMatrix_main = glm::scale(modelMatrix_main, modelInfo.scale);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix_main));
            modelInfo.model->Draw(shaderProgram);
        }

        // --- Render Transparent Objects (Main Pass) ---
        glDepthMask(GL_FALSE);
        renderTransparentObjects(shaderProgram, models, drone.position, isGlassLocation, false);
        renderTransparentObjects(shaderProgram, models, drone.position, isGlassLocation, true);
        glDepthMask(GL_TRUE);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMapTexture);
    glDeleteProgram(depthShaderProgram_global);
    glDeleteProgram(shaderProgram);

    std::cout << "Cleaning up models..." << std::endl;
    for (auto& pair : models) {
        delete pair.second.model;
        pair.second.model = nullptr;
    }
    models.clear();
    std::cout << "Models cleaned up." << std::endl;

    glfwTerminate();
    std::cout << "GLFW terminated." << std::endl;
    return 0;
}
