#pragma once

#include <atomic>
#include <glm/vec2.hpp>

#include "utils/rw_deque.hpp"

namespace UsArMirror {
static const float EPS = std::numeric_limits<float>::epsilon();
/// Global state. Try not to use this often
struct State {
    int viewportWidth = 1280;
    int viewportHeight = 720;
    /// Debug flags
    struct Flags {
        struct {
            bool showWindow = false;
            bool renderKeypoints = false;
        } gesture;
    } flags;
};
} // namespace UsArMirror
