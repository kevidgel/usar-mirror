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

// glm::mat4 getViewMatrixFromExtrinsics(const cv::Mat& R_cv, const cv::Mat& t_cv) {
//     glm::mat4 view(1.0f);

//     // Rotation inverse = transpose
//     view[0][0] =  R_cv.at<float>(0,0);
//     view[0][1] =  R_cv.at<float>(1,0);
//     view[0][2] =  R_cv.at<float>(2,0);

//     view[1][0] =  R_cv.at<float>(0,1);
//     view[1][1] =  R_cv.at<float>(1,1);
//     view[1][2] =  R_cv.at<float>(2,1);

//     view[2][0] =  R_cv.at<float>(0,2);
//     view[2][1] =  R_cv.at<float>(1,2);
//     view[2][2] =  R_cv.at<float>(2,2);

//     // -R^T * t
//     view[3][0] = -(R_cv.at<float>(0,0) * t_cv.at<float>(0) +
//                    R_cv.at<float>(0,1) * t_cv.at<float>(1) +
//                    R_cv.at<float>(0,2) * t_cv.at<float>(2));
//     view[3][1] = -(R_cv.at<float>(1,0) * t_cv.at<float>(0) +
//                    R_cv.at<float>(1,1) * t_cv.at<float>(1) +
//                    R_cv.at<float>(1,2) * t_cv.at<float>(2));
//     view[3][2] = -(R_cv.at<float>(2,0) * t_cv.at<float>(0) +
//                    R_cv.at<float>(2,1) * t_cv.at<float>(1) +
//                    R_cv.at<float>(2,2) * t_cv.at<float>(2));

//     return view;
// }

float computeVerticalFOV(const cv::Mat& K, int imageHeight) {
    float fy = K.at<float>(1, 1);
    return glm::degrees(2.0f * std::atan(static_cast<float>(imageHeight) / (2.0f * fy)));
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

    // cv::dnn::Net faceNet = cv::dnn::readNetFromCaffe(
    //     "deploy.prototxt",
    //     "res10_300x300_ssd_iter_140000.caffemodel");
    
    // auto facemark = cv::face::FacemarkLBF::create();
    // facemark->loadModel("lbfmodel.yaml");
    
    // spdlog::info("Loaded FaceNet and Facemark globally.");
    
    

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
        
        auto activeCam = depthCameraInput;

        activeCam->render();

        glm::vec3 model_pos = glm::vec3(0, 0, 0);

        std::vector<cv::Point3f> landmarks;
        depthCameraInput->getLandmarks3D(landmarks);
        if (landmarks.size() > 0) {
            // model_pos = glm::vec3(landmarks[0].x, landmarks[0].y, landmarks[0].z);
            model_pos = glm::vec3(landmarks[0].x, -landmarks[0].y, -landmarks[0].z);
        }
        // glm::vec3 model_pos = glm::vec3(landmarks[0].x, landmarks[0].y, landmarks[0].z);
        std::cout << "model_pos: " <<landmarks[0].x << ", " << landmarks[0].y << ", " << landmarks[0].z << std::endl;
        auto model_mat = glm::translate(glm::mat4(1.0f), model_pos);
        model_mat = glm::scale(model_mat, glm::vec3(0.2f));
        // model_mat = glm::translate(model_mat, glm::vec3(0.0f, 0.0f, 0.135/2.0f));



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

        
        
        // glm::mat4 model_mat = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
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
        modelRenderer->render(proj, view, model_mat, 0.5f);     

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