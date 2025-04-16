#include "camera.hpp"

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>

namespace UsArMirror {
CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode)
    : state(state), running(true), textureId(-1), rotateCode(rotateCode) {
    // Open capture
    cap.open(idx, cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, state->viewportWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, state->viewportWidth);
    width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    auto framerate = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
    spdlog::info("Webcam {}: width: {}, height: {} framerate: {}", idx, width, height, framerate);
    if (!cap.isOpened()) {
        throw std::runtime_error("Could not open camera!");
    }
    // Setup gl texture
    createGlTexture();
    captureThread = std::thread(&CameraInput::captureLoop, this);
}

CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), textureId(-1) {
    // Open capture
    cap.open(idx, cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, state->viewportWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, state->viewportWidth);
    cap.set(cv::CAP_PROP_BRIGHTNESS, 100);
    width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    auto framerate = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
    spdlog::info("Webcam {}: width: {}, height: {} framerate: {}", idx, width, height, framerate);
    if (!cap.isOpened()) {
        throw std::runtime_error("Could not open camera!");
    }
    // Setup gl texture
    createGlTexture();
    captureThread = std::thread(&CameraInput::captureLoop, this);
}

CameraInput::~CameraInput() {
    running = false;
    if (captureThread.joinable()) {
        captureThread.join();
    }
    cap.release();
}

void CameraInput::createGlTexture() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
}

void CameraInput::captureLoop() {
    while (running) {
        cv::Mat tempFrame;
        if (cap.read(tempFrame)) {
            std::lock_guard lock(frameMutex);
            if (rotateCode.has_value()) {
                rotate(tempFrame, frame, rotateCode.value());
            }
            else {
                frame = tempFrame;
            }
        }
    }
}

void CameraInput::render() {
    cv::Mat frame;
    if (getFrame(frame)) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
        {
            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(1.0f, -1.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(-1.0f, 1.0f);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(1.0f, 1.0f);
        }
        glEnd();
    }
}

bool CameraInput::getFrame(cv::Mat &outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        outputFrame = frame.clone();
        return true;
    }
    return false;
}
} // namespace UsArMirror