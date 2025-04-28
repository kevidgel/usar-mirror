#include "depth_camera.hpp"

#include <iostream>
#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>

#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag25h9.h"


namespace UsArMirror {

inline double standardRad(double t) {
    if (t >= 0.) {
        t = fmod(t+M_PI, 2*M_PI) - M_PI;
    } else {
        t = fmod(t-M_PI, -2*M_PI) + M_PI;
    }
    return t;
    }
    
    void wRo_to_euler(const Eigen::Matrix3d& wRo, double& yaw, double& pitch, double& roll) {
        yaw = standardRad(atan2(wRo(1,0), wRo(0,0)));
        double c = cos(yaw);
        double s = sin(yaw);
        pitch = standardRad(atan2(-wRo(2,0), wRo(0,0)*c + wRo(1,0)*s));
        roll  = standardRad(atan2(wRo(0,2)*s - wRo(1,2)*c, -wRo(0,1)*s + wRo(1,1)*c));
    }
    

DepthCameraInput::DepthCameraInput(const std::shared_ptr<State>& state, int idx)
    : state(state), running(true), textureId(-1), depth_frame(rs2::frame()) {
    try {
        impl = std::make_unique<DepthCameraInputImpl>();
        rs2::config cfg;
        cfg.enable_stream(RS2_STREAM_COLOR, state->viewportWidth, state->viewportHeight, RS2_FORMAT_BGR8, 30);
        cfg.enable_stream(RS2_STREAM_DEPTH, state->viewportWidth, state->viewportHeight, RS2_FORMAT_Z16, 30);
        spdlog::info("Trying to start RealSense pipeline...");
        impl->pipe.start(cfg);

        width = state->viewportWidth;
        height = state->viewportHeight;

        spdlog::info("RealSense camera started: width={}, height={}", width, height);

        faceNet = cv::dnn::readNetFromCaffe(
            "deploy.prototxt",
            "res10_300x300_ssd_iter_140000.caffemodel");

        facemark = cv::face::FacemarkLBF::create();
        facemark->loadModel("lbfmodel.yaml");
        spdlog::info("Loaded OpenCV FacemarkLBF model");

        tagDetector = new AprilTags::TagDetector(AprilTags::tagCodes25h9);

    } catch (const rs2::error& e) {
        throw std::runtime_error(std::string("RealSense error: ") + e.what());
    }

    createGlTexture();
    captureThread = std::thread(&DepthCameraInput::captureLoop, this);
    detectionThread = std::thread(&DepthCameraInput::detectionLoop, this);
}

DepthCameraInput::~DepthCameraInput() {
    running = false;

    if (captureThread.joinable()) captureThread.join();
    if (detectionThread.joinable()) detectionThread.join();

    if (impl) {
        impl->pipe.stop();
    }
}

void DepthCameraInput::createGlTexture() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
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

        // std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}


bool DepthCameraInput::getFrame(cv::Mat& outputFrame) {
    std::lock_guard lock(frameMutex);
    if (!frame.empty()) {
        outputFrame = frame.clone();

        updateExtrinsicsFromAprilTag();

        std::lock_guard faceLock(faceMutex);
        for (size_t i = 0; i < faceBoxes.size(); ++i) {
            // cv::rectangle(outputFrame, faceBoxes[i], cv::Scalar(0, 255, 0), 2);
            // std::cout << landmarkPoints[i].size() << " landmarks for face " << i << std::endl;
            // for (const auto& pt : landmarkPoints[i]) {
            //     cv::circle(outputFrame, pt, 2, cv::Scalar(255, 0, 0), -1);
            //     // if (pt.x >= 0 && pt.x < outputFrame.cols && pt.y >= 0 && pt.y < outputFrame.rows) {
            //     //     std::cout << "Point: " << pt << std::endl;
            //     // } else {
            //     //     std::cout << "Invalid point: " << pt << std::endl;
            //     // }
            // }
        }

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
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.cols, frame.rows, GL_BGR, GL_UNSIGNED_BYTE, frame.data);

        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(1.0f, 1.0f);
        glEnd();
    }
}

std::vector<cv::Point3f> DepthCameraInput::getLandmarks3D() {
    std::lock_guard<std::mutex> lock(landmarkMutex);
    return landmark3D;
}

void DepthCameraInput::updateExtrinsicsFromAprilTag() {
    cv::Mat colorFrame = getLastColorFrame();
    if (colorFrame.empty()) {
        spdlog::warn("No color frame available for AprilTag detection.");
        return;
    }

    // 1. Create AprilTags detector
    // static AprilTags::TagDetector tagDetector(AprilTags::tagCodes25h9); // or 36h11 depending on your tags

    // 2. Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(colorFrame, gray, cv::COLOR_BGR2GRAY);

    // 3. Detect tags
    double t0 = static_cast<double>(cv::getTickCount());
    std::vector<AprilTags::TagDetection> detections = tagDetector->extractTags(gray);
    double dt = (static_cast<double>(cv::getTickCount()) - t0) / cv::getTickFrequency();
    spdlog::info("{} tags detected in {:.3f} seconds", detections.size(), dt);

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
        std::lock_guard lock(extrinsicsMutex);
        extrinsicsMatrix = extrinsic;
    }

    // 8. Log results
    spdlog::info("AprilTag ID: {}", detection.id);
    spdlog::info("Translation (x, y, z) = ({:.3f}, {:.3f}, {:.3f}) meters",
        fixed_trans(0), fixed_trans(1), fixed_trans(2));
    double yaw, pitch, roll;
    wRo_to_euler(fixed_rot, yaw, pitch, roll);
    spdlog::info("Rotation (yaw, pitch, roll) = ({:.3f}, {:.3f}, {:.3f}) radians",
                 yaw, pitch, roll);
}


} // namespace UsArMirror
