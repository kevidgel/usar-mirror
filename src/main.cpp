#define GLFW_INCLUDE_NONE

#include "arduino.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <fontconfig/fontconfig.h>
#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "camera.hpp"
#include "gesture.hpp"
#include "ui.hpp"
#include "model_renderer.hpp"
#include "background_shader.h"
#include "depth_camera.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <opencv2/aruco.hpp>

namespace {
const char *NAME = "UsARMirror";
}

namespace UsArMirror {
glm::mat4 getOpenGLProjectionFromOpenCV(const cv::Mat& K, float width, float height, float near, float far) {
  float fx = K.at<float>(0, 0);
  float fy = K.at<float>(1, 1);
  float cx = K.at<float>(0, 2);
  float cy = K.at<float>(1, 2);

  glm::mat4 proj = glm::mat4(0.0f);

  proj[0][0] = 2.0f * fx / width;
  proj[1][1] = 2.0f * fy / height;
  proj[2][0] = 2.0f * (cx / width) - 1.0f;
  proj[2][1] = 2.0f * (cy / height) - 1.0f;
  proj[2][2] = -(far + near) / (far - near);
  proj[2][3] = -1.0f;
  proj[3][2] = -(2.0f * far * near) / (far - near);

  return proj;
}


glm::mat4 getViewMatrixFromExtrinsics(const cv::Mat& R_cv, const cv::Mat& t_cv) {

    // Assume R_cv is already a 3x3 rotation matrix
    // Assume t_cv is a 3x1 translation vector

    glm::mat4 view(1.0f);

    // Fill rotation part
    view[0][0] =  R_cv.at<float>(0,0);
    view[1][0] =  R_cv.at<float>(1,0);
    view[2][0] =  R_cv.at<float>(2,0);

    view[0][1] = -R_cv.at<float>(0,1);
    view[1][1] = -R_cv.at<float>(1,1);
    view[2][1] = -R_cv.at<float>(2,1);

    view[0][2] = -R_cv.at<float>(0,2);
    view[1][2] = -R_cv.at<float>(1,2);
    view[2][2] = -R_cv.at<float>(2,2);

    // Fill translation part
    view[3][0] =  t_cv.at<float>(0);
    view[3][1] = -t_cv.at<float>(1);
    view[3][2] = -t_cv.at<float>(2);

    return view;
}

void printMat4(const glm::mat4& mat, const std::string& name) {
    std::cout << name << ":\n";
    for (int row = 0; row < 4; ++row) {
        std::cout << "[ ";
        for (int col = 0; col < 4; ++col) {
            std::cout << mat[col][row] << " "; // column-major
        }
        std::cout << "]\n";
    }
}

// void drawDebugPoint(glm::vec3 pos, glm::mat4 view, glm::mat4 proj) {
//     glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
//     model = glm::scale(model, glm::vec3(0.01f));  // small cube

//     glm::mat4 mvp = proj * view * model;

//     glUseProgram(debugShaderProgram);  // you need a simple shader
//     glUniformMatrix4fv(glGetUniformLocation(debugShaderProgram, "uMVP"), 1, GL_FALSE, &mvp[0][0]);

//     glBindVertexArray(debugCubeVAO);  // a VAO for a cube (or sphere)
//     glDrawElements(GL_TRIANGLES, , GL_UNSIGNED_INT, 0);
// }


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

    int filter = 1; //TODO: ADD UI FOR FILTER SELECTION
    std::string filename  = "models/Cube/Cube.gltf";
    if (filter==0){
        filename = "models/Cube/Cube.gltf";
    }else if (filter==1){
        filename = "models/ray-ban_glasses.glb";
    }
    // std::string filename = "models/ray-ban_glasses.glb";
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
    // glEnable(GL_FRAMEBUFFER_SRGB);
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
    auto depthCameraInput = std::make_shared<DepthCameraInput>(state, 6);
    auto cameraInput = std::make_shared<CameraInput>(state, 0);
    // auto cameraInput2 = std::make_shared<CameraInput>(state, 7); // CHANGE THIS NUMBER TO APPROPRIATE
    auto gestureControlPipeline = std::make_shared<GestureControlPipeline>(state, cameraInput);
    auto userInterface = std::make_shared<UserInterface>(state, gestureControlPipeline);
    auto arduino = std::make_shared<Arduino>(state);
    auto modelRenderer = std::make_shared<UsArMirror::ModelRenderer>(state, filename);

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
        
        auto activeCam = cameraInput; // TODO: ADD UI TO CHOOSE ACTIVE CAMERA

        activeCam->render();

        glm::vec3 model_pos = glm::vec3(0, 0, 0);

        std::vector<cv::Point3f> landmarks;
        depthCameraInput->getLandmarks3D(landmarks);

        auto model_mat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));//DEFAULT

        if (filter == 0){ //LOCATE CUBE IN FRONT OF NOSE
            if (landmarks.size() > 0) {
                // model_pos = glm::vec3(landmarks[0].x, landmarks[0].y, landmarks[0].z);
                model_pos = glm::vec3(landmarks[30].x, landmarks[30].y, landmarks[30].z);
            }
            // glm::vec3 model_pos = glm::vec3(landmarks[0].x, landmarks[0].y, landmarks[0].z);
            // std::cout << "model_pos: " <<landmarks[30].x << ", " << landmarks[30].y << ", " << landmarks[30].z << std::endl;
            model_mat = glm::translate(glm::mat4(1.0f), model_pos);
            model_mat = glm::scale(model_mat, glm::vec3(0.2f));
        }else{
            // model_mat = glm::translate(glm::mat4(1.0f), model_pos);
            model_mat = glm::rotate(model_mat, glm::radians(45.0f), glm::vec3(1, 0, 0));
            model_mat = glm::rotate(model_mat, glm::radians(90.0f), glm::vec3(0, 0, 1));
            model_mat = glm::scale(model_mat, glm::vec3(0.01f));
        }
        // model_mat = glm::translate(model_mat, glm::vec3(0.0f, 0.0f, 0.135/2.0f));

        // model_mat = glm::rotate(model_mat, glm::radians(45.0f), glm::vec3(1, 0, 0));
        // model_mat = glm::rotate(model_mat, glm::radians(90.0f), glm::vec3(0, 0, 1));
        // model_mat = glm::scale(model_mat, glm::vec3(0.01f));
        

        // glm::vec3 model_pos(-3, 0, -3);
        // glm::mat4 model_mat = glm::lookAt(glm::vec3(2, 2, 20), model_pos, glm::vec3(0, 1, 0));
        // glm::mat4 model_mat = glm::perspective(glm::radians(45.0f),state->viewportWidth / (float)state->viewportHeight, 0.01f, 1000.0f);
        
        // glm::mat4 proj = glm::perspective(
        //     glm::radians(45.0f),  // 45 degree vertical FOV
        //     640.0f / 480.0f,      // Aspect ratio (width/height)
        //     0.01f,                // Near plane
        //     100.0f                // Far plane
        // );
        // glm::mat4 view = glm::lookAt(
        //     glm::vec3(0.0f, 0.0f, 3.0f),  // Camera position (move 3 units away from origin)
        //     glm::vec3(0.0f, 0.0f, 0.0f),  // Look at the origin
        //     glm::vec3(0.0f, 1.0f, 0.0f)   // Up direction (Y+ is up)
        // );
        // glm::mat4 model_mat = glm::mat4(1.0f); // Identity: no scaling, no movement

        
    
        auto R_vec = activeCam->getExtrinsics().colRange(0, 3).t();
        auto t_vec = activeCam->getExtrinsics().col(3);
        auto view = getViewMatrixFromExtrinsics(R_vec, t_vec);
        auto K = activeCam->intrinsics.getK();

        float scale_x = state->viewportWidth / (float)activeCam->intrinsics.width;
        float scale_y = state->viewportHeight / (float)activeCam->intrinsics.height;

        cv::Mat K_scaled = K.clone();
        K_scaled.at<float>(0, 0) *= scale_x;  // fx
        K_scaled.at<float>(1, 1) *= scale_y;  // fy
        K_scaled.at<float>(0, 2) *= scale_x;  // cx
        K_scaled.at<float>(1, 2) *= scale_y;  // cy

        auto proj = getOpenGLProjectionFromOpenCV(K_scaled, state->viewportWidth, state->viewportHeight, 0.01f, 1000.0f);
        
        // view = glm::mat4(1.0f); // Move the camera back a bit
        // proj = glm::translate(glm::mat4(1.0f), glm::vec3(0.320928f, 0.0750794f, 1.57713f)); // Move the camera back a bit
        // std::cout << "width & height" << std::endl;
        // std::cout << activeCam->intrinsics.width << ", " << activeCam->intrinsics.height << std::endl;
        // std::cout << state->viewportHeight << ", " << state->viewportWidth << std::endl;
        
        // auto proj = glm::perspective(
        //     glm::radians(42.5f),  // 45 degree vertical FOV
        //     activeCam->intrinsics.width / (float)activeCam->intrinsics.height,      // Aspect ratio (width/height)
        //     0.01f,                // Near plane
        //     1000.0f                // Far plane
        // );
        // std::cout << "proj: "<< std::endl;
        // std::cout << computeVerticalFOV(K, (float)activeCam->intrinsics.height) << std::endl;
        // printMat4(proj, "proj");
        // std::cout << "view: "<< std::endl;
        // printMat4(view, "view");
        // std::cout << "model: "<< std::endl;
        // printMat4(model_mat, "model");

        // Combine view and model
        glEnable(GL_DEPTH_TEST);
        modelRenderer->render(proj, view, model_mat, 1.0f); 
        auto mvp = proj * view * model_mat;   
        printMat4(mvp, "mvp");

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