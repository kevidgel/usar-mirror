#define GLFW_INCLUDE_NONE

#include "arduino.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <fontconfig/fontconfig.h>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "camera.hpp"
#include "gesture.hpp"
#include "ui.hpp"
#include "model_renderer.hpp"
#include "background_shader.h"

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
    std::string filename = "models/Cube/Cube.gltf";
    if (argc > 1) filename = argv[1];

    auto state = std::make_shared<State>(); // Shared application state
    spdlog::info("Starting {}", NAME);

    /********** Init glfw, gl **********/
    if (!glfwInit()) {
        spdlog::error("Failed to initialize glfw");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    // glEnable(GL_FRAMEBUFFER_SRGB); //TODO: check if this is needed
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
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
    auto cameraInput = std::make_shared<CameraInput>(state, 6);
    auto gestureControlPipeline = std::make_shared<GestureControlPipeline>(state, cameraInput);
    auto userInterface = std::make_shared<UserInterface>(state, gestureControlPipeline);
    auto arduino = std::make_shared<Arduino>(state);
    auto modelRenderer = std::make_shared<UsArMirror::ModelRenderer>(filename);

    // Render Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // Clear frame
        // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // glClear(GL_COLOR_BUFFER_BIT);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            state->flags.showDebug = true;
        }

        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
            state->flags.showDebug = false;
        }


        cameraInput->render();

        glm::vec3 model_pos(-3, 0, -3);
        glm::mat4 view = glm::lookAt(glm::vec3(2, 2, 20), model_pos, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),state->viewportWidth / (float)state->viewportHeight, 0.01f, 1000.0f);
        
        glEnable(GL_DEPTH_TEST);
        modelRenderer->render(state->viewportWidth * state->viewportScaling, state->viewportHeight * state->viewportScaling, proj, view, 0.5f);
        
        // gestureControlPipeline->render();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

    // //  // Render frontends
        userInterface->render();


        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    spdlog::info("Cleaning up...");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    modelRenderer->cleanup();
    glfwTerminate();
    return EXIT_SUCCESS;
}
} // namespace UsArMirror