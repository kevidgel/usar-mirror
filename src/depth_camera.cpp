#include "depth_camera.hpp"

#include <iostream>
#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

// #include <dlib/opencv.h>
// #include <dlib/image_processing/frontal_face_detector.h>
// #include <dlib/image_processing/render_face_detections.h>
// #include <dlib/image_processing.h>

#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag25h9.h"

namespace UsArMirror {

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx,
    AprilTags::TagDetector* tagDetector, std::mutex* tagMutex)
    : state(state), running(true), textureId(1), depth_frame(rs2::frame()),
    tagDetector(tagDetector), tagMutex(tagMutex) {
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

void DepthCameraInput::tagLoop(){
    while (running) {
        try{
            // std::lock_guard lock(extrinsicsMutex);
            updateExtrinsicsFromAprilTag(); //get Apriltag too
        }
        catch (const std::exception& e) {
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

        cv::Mat blob = cv::dnn::blobFromImage(currentFrame, 1.0, cv::Size(300, 300), cv::Scalar(104.0, 177.0, 123.0), false, false);
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
                
                // std::cout << "depthMat.type() = " << depthMat.type() << std::endl;
                uint16_t d = depthMat.at<uint16_t>(y, x);
                if (d == 0) continue;

                float depth_m = d * 0.001f;
                float px = (x - intr.ppx) / intr.fx;
                float py = (y - intr.ppy) / intr.fy;

                cv::Point3f cameraPoint(px * depth_m, py * depth_m, depth_m);
                auto extrinsic = getExtrinsics(); 

                if (extrinsic.type() != CV_32F && extrinsic.type() != CV_64F) {
                    extrinsic.convertTo(extrinsic, CV_32F);  // Safely convert to float
                }

                auto extrinsic_inv = extrinsic.inv();
                // Transform into world coordinates
                cv::Mat p_c_h = (cv::Mat_<float>(4,1) << cameraPoint.x, cameraPoint.y, cameraPoint.z, 1.0f);
                cv::Mat p_w_h = extrinsic_inv * p_c_h;  // <- extrinsic_inv is your inverted extrinsicsMatrix
                cv::Point3f worldPoint(
                    p_w_h.at<float>(0,0),
                    p_w_h.at<float>(1,0),
                    p_w_h.at<float>(2,0)
                );
                std::cout << "Landmark 3D: " << px * depth_m << ", " << py * depth_m << ", " << depth_m << std::endl;
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

        // std::this_thread::sleep_for(std::chrono::milliseconds(30));
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
        cv::resize(frame, resizedFrame, cv::Size(state->viewportWidth* state->viewportScaling, state->viewportHeight* state->viewportScaling));

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureId);
        // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, resizedFrame.cols, resizedFrame.rows, 0,
            GL_RGB, GL_UNSIGNED_BYTE, resizedFrame.data);

        // Optionally disable depth test if you don't want background to write depth
        glDisable(GL_DEPTH_TEST);

        background.render(textureId, state->viewportWidth* state->viewportScaling, state->viewportHeight* state->viewportScaling);

        glEnable(GL_DEPTH_TEST);
    }
}

std::vector<cv::Point3f> DepthCameraInput::getLandmarks3D() {
    std::lock_guard<std::mutex> lock(landmarkMutex);
    return landmark3D;
}

void DepthCameraInput::updateExtrinsicsFromAprilTag() {
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
        std::lock_guard<std::mutex> lock(*tagMutex); // <-- Lock while using tagDetector
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
