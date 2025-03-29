#pragma once

#include <glm/vec2.hpp>

#include "event.hpp"
#include "utils/rw_deque.hpp"

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
};
} // namespace UsArMirror
