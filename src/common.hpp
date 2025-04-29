#pragma once

#include <glm/vec2.hpp>

#include "event.hpp"
#include "utils/rw_deque.hpp"
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>

#include <Eigen/Dense>


namespace UsArMirror {
static const float EPS = std::numeric_limits<float>::epsilon();
/// Global state. Try not to use this often
struct State {
    float viewportScaling = 1.0f;
    int viewportWidth = 1920;
    int viewportHeight = 1080;
    /// Debug flags
    struct Flags {
        bool showDebug = false;
        struct {
            bool showWindow = false;
        } general;
        struct {
            bool showWindow = false;
            bool renderKeypoints = false;
            bool renderEyeLevel = false;
            bool renderHandCircles = true;
        } gesture;
    } flags;
    
    /// Event queue
    RWDeque<InputEvent> inputEventQueue;
};

struct Intrinsics {
    float fx;
    float fy;
    float cx;
    float cy;
    int width;
    int height;
    std::array<float, 5> dist;
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

inline double standardRad(double t) {
    if (t >= 0.) {
        t = fmod(t+M_PI, 2*M_PI) - M_PI;
    } else {
        t = fmod(t-M_PI, -2*M_PI) + M_PI;
    }
    return t;
}

inline void wRo_to_euler(const Eigen::Matrix3d& wRo, double& yaw, double& pitch, double& roll) {
    yaw = standardRad(atan2(wRo(1,0), wRo(0,0)));
    double c = cos(yaw);
    double s = sin(yaw);
    pitch = standardRad(atan2(-wRo(2,0), wRo(0,0)*c + wRo(1,0)*s));
    roll  = standardRad(atan2(wRo(0,2)*s - wRo(1,2)*c, -wRo(0,1)*s + wRo(1,1)*c));
}
    
} // namespace UsArMirror
