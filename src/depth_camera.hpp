#pragma once

#include <opencv2/opencv.hpp>
#include <glad/glad.h>
#include <memory>
#include <thread>
#include <mutex>
#include <optional>
#include <librealsense2/rs.hpp>

#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include "common.hpp"

#include <vector>
#include <atomic>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/face.hpp>

#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag25h9.h"

namespace UsArMirror {

// struct State {
//     int viewportWidth;
//     int viewportHeight;
// };

struct DepthCameraInputImpl {
    rs2::pipeline pipe;
    rs2::colorizer color_map;
    rs2::frameset frames;
    rs2::frame color_frame;
    rs2::frame depth_frame;
};

class DepthCameraInput {
public:
    explicit DepthCameraInput(const std::shared_ptr<State>& state, int idx);
    ~DepthCameraInput();

    bool getFrame(cv::Mat& outputFrame);
    void render();

    int width, height;
    cv::Mat getLastColorFrame() const;
    rs2::depth_frame getDepth();

    struct Intrinsics {
        float fx = 302.02243162f;
        float fy = 301.80520504f;
        float cx = 324.73866457f;
        float cy = 216.85437825f;
        int width = 640;
        int height = 480;
        std::array<float, 5> dist = {-0.05197052f, -0.11827853f, -0.00824217f, -0.00245351f, 0.09130217f};
        cv::Mat getK() const {
          return (cv::Mat_<float>(3, 3) <<
              fx, 0, cx,
              0, fy, cy,
              0, 0, 1);
        }
        
        cv::Mat getDist() const {
            return cv::Mat(1, 5, CV_32F, (void*)dist.data()).clone();
        }
    
      };

    Intrinsics intrinsics;

    std::vector<cv::Point3f> getLandmarks3D();

    cv::Mat getK() const;
    cv::Mat getDist() const;

    cv::Mat getExtrinsics() const {
        // std::lock_guard lock(extrinsicsMutex);
        return extrinsicsMatrix.clone();
    }

private:
    void createGlTexture();
    void captureLoop();
    void detectionLoop();
    void updateExtrinsicsFromAprilTag();

    cv::Mat extrinsicsMatrix = cv::Mat::eye(4, 4, CV_32F);

    // State
    std::shared_ptr<State> state;
    std::unique_ptr<DepthCameraInputImpl> impl;
    std::atomic<bool> running = true;

    // OpenGL texture
    GLuint textureId;

    // Frame data
    mutable std::mutex frameMutex;
    cv::Mat frame;
    rs2::depth_frame depth_frame;

    // Threads
    std::thread captureThread;
    std::thread detectionThread;

    // Face detection & landmarks
    cv::CascadeClassifier faceDetector;
    cv::Ptr<cv::face::Facemark> facemark;
    std::mutex faceMutex;
    std::vector<cv::Rect> faceBoxes;
    std::vector<std::vector<cv::Point2f>> landmarkPoints;

    cv::dnn::Net faceNet;

    std::vector<cv::Point3f> landmark3D;
    std::mutex landmarkMutex;
    std::mutex extrinsicsMutex;

    AprilTags::TagDetector* tagDetector;

    float tag_size_meters = 0.0736f;  // Set your actual tag size here
};

} // namespace UsArMirror

