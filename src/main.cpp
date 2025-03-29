#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <fontconfig/fontconfig.h>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "camera.hpp"
#include "gesture.hpp"
#include "ui.hpp"

namespace {
const char *NAME = "UsARMirror";
}

namespace UsArMirror {
std::optional<std::string> get_default_font() {
    FcConfig *config = FcInitLoadConfigAndFonts();
    FcPattern *pattern = FcPatternCreate();
    FcObjectSet *object_set = FcObjectSetBuild(FC_FILE, nullptr);
    FcFontSet *font_set = FcFontList(config, pattern, object_set);

    std::string font_path;
    if (font_set && font_set->nfont > 0) {
        FcChar8 *file = nullptr;
        if (FcPatternGetString(font_set->fonts[0], FC_FILE, 0, &file) == FcResultMatch) {
            font_path = reinterpret_cast<const char *>(file);
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }

    FcFontSetDestroy(font_set);
    FcObjectSetDestroy(object_set);
    FcPatternDestroy(pattern);
    FcConfigDestroy(config);

    return font_path;
}

extern "C" int main(int argc, char *argv[]) {
    auto state = std::make_shared<State>(); // Shared application state
    spdlog::info("Starting {}", NAME);

    /********** Init glfw, gl **********/
    if (!glfwInit()) {
        spdlog::error("Failed to initialize glfw");
        return EXIT_FAILURE;
    }

    GLFWwindow *window;
    window = glfwCreateWindow(state->viewportWidth * state->viewportScaling,
                              state->viewportHeight * state->viewportScaling, NAME, nullptr, nullptr);
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
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Setup font
    auto font_path_res = get_default_font();
    if (font_path_res.has_value()) {
        std::string font_path = font_path_res.value();
        spdlog::debug("Using font: {}", font_path);
        ImFontConfig font_config;
        io.Fonts->AddFontFromFileTTF(font_path_res.value().c_str(), 16.0f, &font_config);
    } else {
        spdlog::warn("Could not find a default font, using the ImGui default font.");
    }

    // Launch tasks
    auto cameraInput = std::make_shared<CameraInput>(state, 2);
    auto gestureControlPipeline = std::make_shared<GestureControlPipeline>(state, cameraInput);
    auto userInterface = std::make_shared<UserInterface>(state, gestureControlPipeline);

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear frame
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            state->flags.showDebug = true;
        }

        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
            state->flags.showDebug = false;
        }

        // Render frontends
        userInterface->render();
        cameraInput->render();
        gestureControlPipeline->render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    spdlog::info("Cleaning up...");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return EXIT_SUCCESS;
}
} // namespace UsArMirror