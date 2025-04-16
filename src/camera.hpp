#pragma once

#include <opencv2/opencv.hpp>
#include <glad/glad.h>
#include <thread>

#include "common.hpp"

namespace UsArMirror {
class CameraInput {
  public:
    explicit CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode);
    explicit CameraInput(const std::shared_ptr<State>& state, int idx);
    ~CameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    int width, height;

  private:
    std::shared_ptr<State> state;
    bool running;
    cv::VideoCapture cap;
    cv::Mat frame;
    std::mutex frameMutex;
    std::thread captureThread;
    GLuint textureId;
    std::optional<int> rotateCode = std::nullopt;

    void createGlTexture();
    void captureLoop();
};

} // namespace UsArMirror