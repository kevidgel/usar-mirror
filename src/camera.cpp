#include "camera.hpp"

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <libudev.h>
#include <string>

std::string getSerialFromDevicePath(const std::string& devicePath) {
    struct udev *udev = udev_new();
    if (!udev) return "";
  
    struct udev_device *dev = udev_device_new_from_subsystem_sysname(udev, "video4linux", devicePath.substr(devicePath.find_last_of("/") + 1).c_str());
    if (!dev) {
        udev_unref(udev);
        return "";
    }
  
    struct udev_device *usb = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    std::string serial;
    if (usb) {
        const char* ser = udev_device_get_sysattr_value(usb, "serial");
        if (ser) serial = ser;
    }
  
    udev_device_unref(dev);
    udev_unref(udev);
    return serial;
  }

namespace UsArMirror {

CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode
    )
    : state(state), running(true), rotateCode(rotateCode){
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
    
    std::string devicePath = "/dev/video" + std::to_string(idx);
    std::string serial = getSerialFromDevicePath(devicePath);
    spdlog::info("Camera {} serial: {}", idx, serial);
    intrinsics = cameraIntrinsics.find(serial)->second;

    arucoDict = cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_250));

    createGlTexture();
    captureThread = std::thread(&CameraInput::captureLoop, this);
    detectionThread = std::thread(&CameraInput::detectionLoop, this);
}

CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx
    )
    : state(state), running(true), rotateCode(std::nullopt){
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
        for (int i = 0; i <= 10; ++i) {
            if (i == idx) continue;
            if (cap.open(i, cv::CAP_V4L2)) {
                cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
                cap.set(cv::CAP_PROP_FRAME_WIDTH, state->viewportWidth);
                cap.set(cv::CAP_PROP_FRAME_HEIGHT, state->viewportWidth);
                spdlog::info("Webcam {}: width: {}, height: {} framerate: {}", idx, width, height, framerate);
                spdlog::warn("Fallback successful: opened camera at index {}", i);
                idx = i;
                break;
            }
        }

        if (!cap.isOpened()) {
            throw std::runtime_error("Could not open camera at index " + std::to_string(idx));
        }
    }

    std::string devicePath = "/dev/video" + std::to_string(idx);
    std::string serial = getSerialFromDevicePath(devicePath);
    spdlog::info("Camera {} serial: {}", idx, serial);
    intrinsics = cameraIntrinsics.find(serial)->second;

    arucoDict = cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_250));

    createGlTexture();
    captureThread = std::thread(&CameraInput::captureLoop, this);
    detectionThread = std::thread(&CameraInput::detectionLoop, this);
}

CameraInput::~CameraInput() {
    running = false;
    if (captureThread.joinable()) {
        captureThread.join();
    }
    if (detectionThread.joinable()) {
        detectionThread.join();
    }
    cap.release();
}

void CameraInput::createGlTexture() {
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

void CameraInput::detectionLoop(){
    while (running){
        std::lock_guard lock(extrinsicsMutex);
        updateExtrinsicsFromAruco(); //get Apriltag too
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CameraInput::render() {
    cv::Mat frame;
    if (getFrame(frame)) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureId);
        // std::cout<<"frame size" << frame.cols << " " << frame.rows << std::endl;
        // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0,
            GL_RGB, GL_UNSIGNED_BYTE, frame.data);

        // Optionally disable depth test if you don't want background to write depth
        glBindTexture(GL_TEXTURE_2D, 0);
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


void CameraInput::updateExtrinsicsFromAruco() {
    cv::Mat frame;
    if (!getFrame(frame)) {
        // spdlog::warn("No color frame available for ArUco detection.");
        return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    cv::aruco::detectMarkers(gray, arucoDict, corners, ids);

    if (ids.empty()) {
        // spdlog::warn("No ArUco markers detected.");
        return;
    }

    auto intr = intrinsics;
    cv::Mat K = (cv::Mat_<double>(3, 3) << intr.fx, 0, intr.cx,
                                           0, intr.fy, intr.cy,
                                           0, 0, 1);
    cv::Mat dist = intr.getDist();
    std::vector<cv::Vec3d> rvecs, tvecs;
    cv::aruco::estimatePoseSingleMarkers(corners, tag_size_meters, K, dist, rvecs, tvecs);

    if (!rvecs.empty()) {
        cv::Mat R_cv;
        cv::Rodrigues(rvecs[0], R_cv);

        cv::Mat extrinsic = cv::Mat::eye(4, 4, CV_32F);
        R_cv.convertTo(extrinsic(cv::Rect(0, 0, 3, 3)), CV_32F);
        cv::Mat(tvecs[0]).convertTo(extrinsic(cv::Rect(3, 0, 1, 3)), CV_32F);

        extrinsicsMatrix = extrinsic;

        // spdlog::info("ArUco ID: {}", ids[0]);
    }
}

} // namespace UsArMirror