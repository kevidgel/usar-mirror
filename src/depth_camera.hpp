#pragma once

#include <glad/glad.h>
#include <librealsense2/rs.hpp>
#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <optional>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <vector>

#include "background_shader.h"
#include "common.hpp"

#include <atomic>
#include <opencv2/aruco.hpp>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/face.hpp>
#include <opencv2/objdetect.hpp>
#include <vector>

namespace UsArMirror {

struct DepthCameraInputImpl {
    rs2::pipeline pipe;
    rs2::colorizer color_map;
    rs2::frameset frames;
    rs2::frame color_frame;
    rs2::frame depth_frame;
};

class DepthCameraInput {
  public:
    DepthCameraInput(const std::shared_ptr<State> &state);
    ~DepthCameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    // int width, height;
    cv::Mat getLastColorFrame() const;
    rs2::depth_frame getDepth();

    Intrinsics intrinsics = Intrinsics{
        .fx = 1.36714743e+03f,
        .fy = 1.34790890e+03f,
        .cx = 3.28297800e+02f,
        .cy = 2.42004385e+02f,
        .width = 640,
        .height = 480,
        .dist = {-1.59267188e+00, 2.73637462e+01, -4.13155799e-02, -2.57490870e-02, -1.89969469e+02}
    };

    void getLandmarks3D(std::vector<cv::Point3f> &out);

    // cv::Mat getK() const;
    // cv::Mat getDist() const;

    cv::Mat getExtrinsics() const {
        // std::lock_guard lock(extrinsicsMutex);
        return extrinsicsMatrix.clone();
    }

    int width = 640;
    int height = 480;

  private:
    void createGlTexture();
    void captureLoop();
    void detectionLoop();
    void tagLoop();
    void updateExtrinsicsFromAruco();

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
    cv::Mat depthMat;
    rs2::depth_frame depth_frame;

    // Threads
    std::thread captureThread;
    std::thread detectionThread;
    std::thread tagThread;

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

    BackgroundShader background;

    cv::Ptr<cv::aruco::Dictionary> arucoDict;

    float tag_size_meters = 0.135f; // Set your actual tag size here
};

} // namespace UsArMirror
