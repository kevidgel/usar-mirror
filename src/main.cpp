/**
 * @file main.cpp
 */

#define GLFW_INCLUDE_NONE

#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "gesture.hpp"
#include "ui.hpp"

namespace {
const char *NAME = "UsARMirror";
}

namespace UsArMirror {
extern "C" int main(int argc, char *argv[]) {
    auto *state = new State(); // Shared application state
    spdlog::info("Starting {}", NAME);

    /********** Init glfw, gl **********/
    if (!glfwInit()) {
        spdlog::error("Failed to initialize glfw");
        return EXIT_FAILURE;
    }

    GLFWwindow *window;
    window = glfwCreateWindow(state->viewportWidth, state->viewportHeight, NAME, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        spdlog::error("Failed to create window");
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        spdlog::error("Failed to load glad");
        return EXIT_FAILURE;
    }

    const GLubyte *glVersion = glGetString(GL_VERSION);
    const GLubyte *glRenderer = glGetString(GL_RENDERER);
    spdlog::info("GL_VERSION: {}", reinterpret_cast<const char *>(glVersion));
    spdlog::info("GL_RENDERER: {}", reinterpret_cast<const char *>(glRenderer));

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glfwSwapInterval(0);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Launch tasks
    UserInterface userInterface(state);
    auto cameraInput = std::make_shared<CameraInput>(state, 0, 1280, 720);
    GestureControlPipeline gestureControlPipeline(state, cameraInput);

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear frame
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render frontends
        userInterface.render();
        cameraInput->render();
        gestureControlPipeline.render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }


    // Cleanup
    spdlog::info("Cleaning up...");
    delete state;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return EXIT_SUCCESS;
}
} // namespace UsArMirror