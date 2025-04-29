#include "camera.hpp"

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <libudev.h>
#include <string>

#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag25h9.h"


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

CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode,
    AprilTags::TagDetector* tagDetector, std::mutex* tagMutex)
    : state(state), running(true), rotateCode(rotateCode),
    tagDetector(tagDetector), tagMutex(tagMutex) {
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

    createGlTexture();
    captureThread = std::thread(&CameraInput::captureLoop, this);
    detectionThread = std::thread(&CameraInput::detectionLoop, this);
}

CameraInput::CameraInput(const std::shared_ptr<State>& state, int idx,
    AprilTags::TagDetector* tagDetector, std::mutex* tagMutex)
    : state(state), running(true), rotateCode(std::nullopt),
    tagDetector(tagDetector), tagMutex(tagMutex) {
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
        updateExtrinsicsFromAprilTag(); //get Apriltag too
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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


void CameraInput::updateExtrinsicsFromAprilTag() {
    cv::Mat frame;
    if (!getFrame(frame)) {
        spdlog::warn("No color frame available for AprilTag detection.");
        return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    double t0 = static_cast<double>(cv::getTickCount());

    std::vector<AprilTags::TagDetection> detections;
    {
        std::lock_guard<std::mutex> lock(*tagMutex); // <-- lock the shared TagDetector
        detections = tagDetector->extractTags(gray);
    }

    double dt = (static_cast<double>(cv::getTickCount()) - t0) / cv::getTickFrequency();

    if (detections.empty()) {
        spdlog::warn("No AprilTags detected.");
        return;
    }

    // 4. Only use the first detection for now
    AprilTags::TagDetection& detection = detections[0];

    // 5. Recover relative pose
    Eigen::Vector3d translation;
    Eigen::Matrix3d rotation;
    detection.getRelativeTranslationRotation(
        tag_size_meters,   // tag size in meters (adjust to your actual tag size)
        intrinsics.fx, intrinsics.fy,
        intrinsics.cx, intrinsics.cy,
        translation, rotation);

    // 6. Convert rotation matrix to OpenCV
    Eigen::Matrix3d F;
    F << 0, 1, 0,
        0, 0, -1,
        1, 0, 0;
    Eigen::Matrix3d fixed_rot = F * rotation;  // fix AprilTag frame convention

    cv::Mat R_cv(3, 3, CV_64F);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            R_cv.at<double>(i, j) = fixed_rot(i, j);

    cv::Mat rvec;
    cv::Rodrigues(R_cv, rvec);
    
    Eigen::Vector3d fixed_trans = F * translation;
    cv::Mat tvec = (cv::Mat_<double>(3,1) << fixed_trans(0), fixed_trans(1), fixed_trans(2));

    // 7. Build a 4x4 transformation matrix
    cv::Mat extrinsic = cv::Mat::eye(4, 4, CV_32F);
    R_cv.convertTo(extrinsic(cv::Rect(0, 0, 3, 3)), CV_32F);
    tvec.convertTo(extrinsic(cv::Rect(3, 0, 1, 3)), CV_32F);

    {
        // std::lock_guard lock(extrinsicsMutex);
        extrinsicsMatrix = extrinsic;
    }

    // 8. Log results
    // spdlog::info("AprilTag ID: {}", detection.id);
    // spdlog::info("Translation (x, y, z) = ({:.3f}, {:.3f}, {:.3f}) meters",
    //     fixed_trans(0), fixed_trans(1), fixed_trans(2));
    // double yaw, pitch, roll;
    // wRo_to_euler(fixed_rot, yaw, pitch, roll);
    // spdlog::info("Rotation (yaw, pitch, roll) = ({:.3f}, {:.3f}, {:.3f}) radians",
    //              yaw, pitch, roll);
}
} // namespace UsArMirror