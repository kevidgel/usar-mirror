#pragma once

#include <glm/vec2.hpp>

#include "event.hpp"
#include "utils/rw_deque.hpp"

namespace UsArMirror {
static const float EPS = std::numeric_limits<float>::epsilon();
/// Global state. Try not to use this often
struct State {
    int viewportWidth = 640*2;
    int viewportHeight = 480*2;
    /// Debug flags
    struct Flags {
        struct {
            bool showWindow = false;
            bool renderKeypoints = false;
            bool renderEyeLevel = false;
        } gesture;
    } flags;
    /// Event queue
    RWDeque<InputEvent> inputEventQueue;
};
} // namespace UsArMirror
