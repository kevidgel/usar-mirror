#pragma once

#include <opencv2/opencv.hpp>
#include <thread>

#include "common.hpp"

#include <glad/glad.h>

namespace UsArMirror {
class CameraInput {
  public:
    explicit CameraInput(State *state, int idx, int width, int height);
    ~CameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    int width, height;

  private:
    State *state;
    bool running;
    cv::VideoCapture cap;
    cv::Mat frame;
    std::mutex frameMutex;
    std::thread captureThread;
    GLuint textureId;

    void captureLoop();
};

} // namespace UsArMirror