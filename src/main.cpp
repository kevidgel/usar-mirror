/**
 * @file main.cpp
 */

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "common.hpp"
#include "pipeline.hpp"

namespace UsArMirror {
/// callback for opengl
void GLAPIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                               const GLchar *message, const void *user_param) {
    if (type != GL_DEBUG_TYPE_ERROR) {
        spdlog::debug("GL CALLBACK type = 0x{} severity = 0x{}, message = {}\n", type, severity, message);
    } else {
        spdlog::error("GL CALLBACK type = 0x{} severity = 0x{}, message = {}\n", type, severity, message);
    }
}

extern "C" int main(int argc, char *argv[]) {
    spdlog::info("Starting {}", NAME);
    // Create GLFW window
    GLFWwindow *window;

    if (!glfwInit()) {
        spdlog::error("Failed to initialize glfw");
        return EXIT_FAILURE;
    }

    window = glfwCreateWindow(640, 480, NAME, nullptr, nullptr);
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

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallback, nullptr);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glfwSwapInterval(0);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();


    // Start pipeline.
    Pipeline pipeline;
    pipeline.start();

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Do something...
        ImGui::ShowDemoWindow();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return EXIT_SUCCESS;
}
} // namespace UsArMirror