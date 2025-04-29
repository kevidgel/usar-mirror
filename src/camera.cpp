#include "camera.hpp"

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>



namespace UsArMirror {
CameraInput::CameraInput(const std::shared_ptr<State>& state, 
 int idx, int rotateCode)
    : state(state), running(true), rotateCode(rotateCode) {
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
    : state(state), running(true){
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
        // Optional fallback (if you want to try index 0 or others)
        for (int i = 0; i <= 10; ++i) {
            if (i == idx) continue;
            if (cap.open(i, cv::CAP_V4L2)) {
                spdlog::warn("Fallback successful: opened camera at index {}", i);
                idx = i;
                break;
            }
        }

        if (!cap.isOpened()) {
            throw std::runtime_error("Could not open camera at index " + std::to_string(idx));
        }
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
    // glGenTextures(1, &textureId);
    // glBindTexture(GL_TEXTURE_2D, textureId);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
    // background = BackgroundShader();
    // glActiveTexture(GL_TEXTURE1);
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureId);
        // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0,
            GL_RGB, GL_UNSIGNED_BYTE, frame.data);

        // Optionally disable depth test if you don't want background to write depth
        glDisable(GL_DEPTH_TEST);

        background.render(textureId, state->viewportWidth, state->viewportHeight);

        glEnable(GL_DEPTH_TEST);
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