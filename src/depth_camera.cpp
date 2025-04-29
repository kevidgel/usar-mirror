#include "depth_camera.hpp"

#include <iostream>
#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <opencv2/aruco.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

namespace UsArMirror {

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), textureId(1), depth_frame(rs2::frame()) {
    try {
        impl = std::make_unique<DepthCameraInputImpl>();

        rs2::config cfg;
        cfg.enable_stream(RS2_STREAM_COLOR, width, height, RS2_FORMAT_BGR8, 30);
        cfg.enable_stream(RS2_STREAM_DEPTH, width, height, RS2_FORMAT_Z16, 30);
        spdlog::info("Trying to start RealSense pipeline...");
        impl->pipe.start(cfg);

        spdlog::info("RealSense camera started: width={}, height={}", width, height);

        faceNet = cv::dnn::readNetFromCaffe(
            "deploy.prototxt",
            "res10_300x300_ssd_iter_140000.caffemodel");

        facemark = cv::face::FacemarkLBF::create();
        facemark->loadModel("lbfmodel.yaml");
        spdlog::info("Loaded OpenCV FacemarkLBF model");

        arucoDict = cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_250));

    } catch (const rs2::error& e) {
        throw std::runtime_error(std::string("RealSense error: ") + e.what());
    }

    createGlTexture();
    captureThread = std::thread(&DepthCameraInput::captureLoop, this);
    detectionThread = std::thread(&DepthCameraInput::detectionLoop, this);
    tagThread = std::thread(&DepthCameraInput::tagLoop, this);
}

DepthCameraInput::~DepthCameraInput() {
    running = false;

    if (captureThread.joinable()) captureThread.join();
    if (detectionThread.joinable()) detectionThread.join();
    if (tagThread.joinable()) tagThread.join();

    if (impl) {
        impl->pipe.stop();
    }
}

void DepthCameraInput::createGlTexture() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void DepthCameraInput::captureLoop() {
    while (running) {
        if (!impl) continue;

        if (impl->pipe.poll_for_frames(&impl->frames)) {
            rs2::video_frame color = impl->frames.get_color_frame();
            rs2::depth_frame depth = impl->frames.get_depth_frame();

            if (color) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(color.get_data());
                cv::Mat raw(color.get_height(), color.get_width(), CV_8UC3, (void*)data, cv::Mat::AUTO_STEP);
                std::lock_guard lock(frameMutex);
                frame = raw.clone();
            }

            if (depth) {
                std::lock_guard lock(frameMutex);
                depth_frame = depth;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void DepthCameraInput::tagLoop() {
    while (running) {
        try {
            updateExtrinsicsFromAruco();
        } catch (const std::exception& e) {
            spdlog::error("Error in tagLoop: {}", e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void DepthCameraInput::detectionLoop() {
    while (running) {
        cv::Mat currentFrame, depthMat;
        {
            std::lock_guard lock(frameMutex);
            if (frame.empty() || !depth_frame) continue;
            currentFrame = frame.clone();
            depthMat = cv::Mat(depth_frame.get_height(), depth_frame.get_width(), CV_16UC1,
                               (void*)depth_frame.get_data(), cv::Mat::AUTO_STEP).clone();
        }

        auto start = std::chrono::high_resolution_clock::now();

        cv::Mat blob = cv::dnn::blobFromImage(currentFrame, 1.0, cv::Size(300, 300),
                                              cv::Scalar(104.0, 177.0, 123.0), false, false);
        faceNet.setInput(blob);
        cv::Mat detections = faceNet.forward();

        std::vector<cv::Rect> faces;
        cv::Mat detectionMat(detections.size[2], detections.size[3], CV_32F, detections.ptr<float>());

        for (int i = 0; i < detectionMat.rows; ++i) {
            float confidence = detectionMat.at<float>(i, 2);
            if (confidence > 0.9f) {
                int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * currentFrame.cols);
                int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * currentFrame.rows);
                int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * currentFrame.cols);
                int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * currentFrame.rows);
                faces.emplace_back(cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)));
            }
        }

        std::vector<std::vector<cv::Point2f>> landmarks;
        if (facemark->fit(currentFrame, faces, landmarks) && !landmarks.empty()) {
            std::vector<cv::Point3f> points3D;
            auto intr = impl->pipe.get_active_profile()
                            .get_stream(RS2_STREAM_COLOR)
                            .as<rs2::video_stream_profile>()
                            .get_intrinsics();

            for (const auto& pt : landmarks[0]) {
                int x = static_cast<int>(pt.x);
                int y = static_cast<int>(pt.y);
                if (x < 0 || x >= depthMat.cols || y < 0 || y >= depthMat.rows) continue;

                uint16_t d = depthMat.at<uint16_t>(y, x);
                if (d == 0) continue;

                float depth_m = d * 0.001f;
                float px = (x - intr.ppx) / intr.fx;
                float py = (y - intr.ppy) / intr.fy;

                points3D.emplace_back(cv::Point3f(px * depth_m, py * depth_m, depth_m));
            }

            {
                std::lock_guard lock(faceMutex);
                faceBoxes = faces;
                landmarkPoints = landmarks;
                landmark3D = points3D;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        spdlog::info("Detect face took {} µs", duration);
    }
}

bool DepthCameraInput::getFrame(cv::Mat& outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        outputFrame = frame.clone();
        return true;
    }
    return false;
}

rs2::depth_frame DepthCameraInput::getDepth() {
    std::lock_guard lock(frameMutex);
    return depth_frame;
}

cv::Mat DepthCameraInput::getLastColorFrame() const {
    return frame.empty() ? cv::Mat() : frame.clone();
}

void DepthCameraInput::render() {
    cv::Mat frame;
    if (getFrame(frame)) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        cv::Mat resizedFrame;
        cv::resize(frame, resizedFrame, cv::Size(state->viewportWidth * state->viewportScaling, state->viewportHeight * state->viewportScaling));

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, resizedFrame.cols, resizedFrame.rows, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, resizedFrame.data);

        glDisable(GL_DEPTH_TEST);
        background.render(textureId, state->viewportWidth * state->viewportScaling, state->viewportHeight * state->viewportScaling);
        glEnable(GL_DEPTH_TEST);
    }
}

void DepthCameraInput::getLandmarks3D(std::vector<cv::Point3f>& out) {
    std::lock_guard<std::mutex> lock(faceMutex);
    out = landmark3D;
}

void DepthCameraInput::updateExtrinsicsFromAruco() {
    cv::Mat frame;
    if (!getFrame(frame)) {
        spdlog::warn("No color frame available for ArUco detection.");
        return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    cv::aruco::detectMarkers(gray, arucoDict, corners, ids);

    if (ids.empty()) {
        spdlog::warn("No ArUco markers detected.");
        return;
    }

    auto intr = impl->pipe.get_active_profile()
                        .get_stream(RS2_STREAM_COLOR)
                        .as<rs2::video_stream_profile>()
                        .get_intrinsics();
    cv::Mat K = (cv::Mat_<double>(3, 3) << intr.fx, 0, intr.ppx,
                                           0, intr.fy, intr.ppy,
                                           0, 0, 1);
    cv::Mat dist = cv::Mat::zeros(1, 5, CV_64F);

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