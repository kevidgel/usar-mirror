// main.cpp

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glad/glad.h>
#include <iostream>

#include "constants.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace UsArMirror {
void GLAPIENTRY debug_callback(GLenum source, GLenum type, GLuint id,
                               GLenum severity, GLsizei length,
                               const GLchar *message, const void *user_param) {
  if (type != GL_DEBUG_TYPE_ERROR) {
    std::cout << message << std::endl;
  } else {
    std::cout << "error: " << message << std::endl;
  }
}

extern "C" int main(int argc, char *argv[]) {
  // Create GLFW window
  GLFWwindow *window;

  if (!glfwInit()) {
    return EXIT_FAILURE;
  }

  window = glfwCreateWindow(640, 480, UsArMirror::NAME, NULL, NULL);
  if (!window) {
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "failed to get glad" << std::endl;
    return EXIT_FAILURE;
  }

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(debug_callback, nullptr);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glfwSwapInterval(0);

  // Setup ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();

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