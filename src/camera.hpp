#pragma once

#include <opencv2/opencv.hpp>
#include <glad/glad.h>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <string>

#include "common.hpp"
#include "background_shader.h"

#include <opencv2/aruco.hpp>

namespace UsArMirror {
class CameraInput {
  public:
    CameraInput(const std::shared_ptr<State>& state, int idx, int rotateCode);
    CameraInput(const std::shared_ptr<State>& state, int idx);
    ~CameraInput();

    bool getFrame(cv::Mat &outputFrame);
    void render();

    int width, height;

    cv::Mat getExtrinsics() const {
      // std::lock_guard lock(extrinsicsMutex);
      return extrinsicsMatrix.clone();
    }


    std::unordered_map<std::string, Intrinsics> cameraIntrinsics = {
        {
            "5EC0DDDF", // Right camera
            Intrinsics{
                .fx = 1337.08201f,
                .fy = 1328.98040f,
                .cx = 965.106161f,
                .cy = 559.241491f,
                .width = 1920,
                .height = 1080,
                .dist = {-0.03591724f, -0.06893587f, 0.00183622f, 0.00428361f, 0.04353619f}
            }
        },
        {
            "836ABDDF", // Left camera
            Intrinsics{
                .fx = 1433.58795f,
                .fy = 1427.54584f,
                .cx = 949.041155f,
                .cy = 599.118984f,
                .width = 1920,
                .height = 1080,
                .dist = {-0.00731259f, 0.11281706f, 0.00967439f, -0.0019929f, -0.39362477f}
            }
        }
    };

    Intrinsics intrinsics;

    cv::Mat getK() const;
    cv::Mat getDist() const;

    // 5EC0DDDF (right camera)
//     [[1.33708201e+03 0.00000000e+00 9.65106161e+02]
//  [0.00000000e+00 1.32898040e+03 5.59241491e+02]
//  [0.00000000e+00 0.00000000e+00 1.00000000e+00]] [[-0.03591724 -0.06893587  0.00183622  0.00428361  0.04353619]]


    // 836ABDDF (left camera)
    //     [[1.43358795e+03 0.00000000e+00 9.49041155e+02]
//  [0.00000000e+00 1.42754584e+03 5.99118984e+02]
//  [0.00000000e+00 0.00000000e+00 1.00000000e+00]] [[-0.00731259  0.11281706  0.00967439 -0.0019929  -0.39362477]]


  private:
    std::shared_ptr<State> state;
    bool running;
    cv::VideoCapture cap;
    cv::Mat frame;
    std::mutex frameMutex;
    std::thread captureThread;
    std::thread detectionThread;
    GLuint textureId;
    std::optional<int> rotateCode = std::nullopt;
    BackgroundShader background;
    void createGlTexture();
    void captureLoop();
    void detectionLoop();
    void updateExtrinsicsFromAruco();


    cv::Ptr<cv::aruco::Dictionary> arucoDict;

    float tag_size_meters = 0.135f; 

    std::mutex extrinsicsMutex;
    cv::Mat extrinsicsMatrix = cv::Mat::eye(4, 4, CV_32F);
};

} // namespace UsArMirror