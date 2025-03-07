/**
 * @file main.cpp
 */

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "pipeline.hpp"
#include "rw_deque.hpp"

namespace {
const char *NAME = "UsARMirror";
}

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

void startPipeline(State *state) {
    Pipeline pipeline(state);
    pipeline.start();
}

/// Assign int to arbitrary color
glm::vec3 intToRgb(uint i) {
    const float r = static_cast<float>((i * 81059059) % 256) / 255.f;
    const float g = static_cast<float>((i * 68995967) % 256) / 255.f;
    const float b = static_cast<float>((i * 41394649) % 256) / 255.f;
    return {r, g, b};
}

extern "C" int main(int argc, char *argv[]) {
    spdlog::info("Starting {}", NAME);

    /********** Init glfw, gl **********/
    if (!glfwInit()) {
        spdlog::error("Failed to initialize glfw");
        return EXIT_FAILURE;
    }

    GLFWwindow *window;
    window = glfwCreateWindow(1280, 720, NAME, nullptr, nullptr);
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
    glDebugMessageCallback(debugCallback, nullptr); // dont use this on mac
    glEnable(GL_FRAMEBUFFER_SRGB);
    glfwSwapInterval(0);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    /********** Launch tasks **********/
    auto *state = new State(); // Shared application state
    std::thread keypointExtraction(startPipeline, state);

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

        glPointSize(10.0);
        glBegin(GL_POINTS);
        auto o = state->keypointQueue.peek_front();
        if (o.has_value()) {
            for (const auto& [i, point] : o.value().val) {
                glm::vec3 color = intToRgb(i);
                glColor3f(color.x, color.y, color.z);
                // map [0, 1920] x [0, 1080] --> [1.0, -1.0] x [1.0, -1.0]
                glVertex2f(1.0f - 2.0f * (point.x / 1920.f), 1.0f - 2.0f * (point.y / 1080.f));
            }
        }
        glEnd();
        state->keypointQueue.evict(std::chrono::milliseconds(1200));

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    state->die = true;
    keypointExtraction.join();
    delete state;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return EXIT_SUCCESS;
}
} // namespace UsArMirror