#include "camera.hpp"

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

namespace UsArMirror {
CameraInput::CameraInput(State *state, int idx, int width, int height)
    : state(state), running(true), width(width), height(height) {
    // Setup gl texture
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);

    // Open capture
    cap.open(idx, cv::CAP_V4L2);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap.set(cv::CAP_PROP_EXPOSURE, 0.01);
    if (!cap.isOpened()) {
        throw std::runtime_error("Could not open camera!");
    }
    captureThread = std::thread(&CameraInput::captureLoop, this);
}

CameraInput::~CameraInput() {
    running = false;
    if (captureThread.joinable()) {
        captureThread.join();
    }
    cap.release();
}

void CameraInput::captureLoop() {
    while (running) {
        cv::Mat tempFrame;
        if (cap.read(tempFrame)) {
            std::lock_guard lock(frameMutex);
            frame = tempFrame.clone();
        }
    }
}

void CameraInput::render() {
    cv::Mat frame;
    if (getFrame(frame)) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);

        glBegin(GL_QUADS);
        {
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