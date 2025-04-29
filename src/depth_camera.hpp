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
#include "background_shader.h"

#include <vector>
#include <atomic>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/face.hpp>
#include <opencv2/aruco.hpp>



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
    DepthCameraInput(const std::shared_ptr<State>& state, int idx);
    ~DepthCameraInput();

    bool getFrame(cv::Mat& outputFrame);
    void render();

    // int width, height;
    cv::Mat getLastColorFrame() const;
    rs2::depth_frame getDepth();


    Intrinsics intrinsics = Intrinsics{
        .fx = 148.22530571f,
        .fy = 149.44816246f,
        .cx = 291.64137733f,
        .cy = 216.22790337,
        .width = 640,
        .height = 480,
        .dist = {-1.43234225e-02f, -5.47372135e-04f, 4.36393052e-04f, -3.06948268e-04f, 5.58948300e-05f}
    };

    void getLandmarks3D(std::vector<cv::Point3f>& out);

    // cv::Mat getK() const;
    // cv::Mat getDist() const;

    cv::Mat getExtrinsics() const {
        // std::lock_guard lock(extrinsicsMutex);
        return extrinsicsMatrix.clone();
    }

private:
    void createGlTexture();
    void captureLoop();
    void detectionLoop();
    void tagLoop();
    void updateExtrinsicsFromAruco();

    int width = 640;
    int height = 480;

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
    // std::mutex extrinsicsMutex;

    BackgroundShader background;


    cv::Ptr<cv::aruco::Dictionary> arucoDict;

    float tag_size_meters = 0.135f;  // Set your actual tag size here
};

} // namespace UsArMirror

