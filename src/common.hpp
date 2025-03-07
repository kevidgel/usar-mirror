/**
 * @file constants.hpp
 */

#pragma once

#include <glm/vec2.hpp>

#include "rw_deque.hpp"

namespace UsArMirror {
static const float EPS = std::numeric_limits<float>::epsilon();
struct State {
    /// Set to true when render thread is shutting down
    std::atomic_bool die = false;
    /// (de)queue of (keypointId, position) pairs.
    RWDeque<std::vector<std::pair<uint8_t, glm::vec2>>> keypointQueue;
    /// (de)queue of (keypointId, velocity) pairs.
    RWDeque<std::vector<std::pair<uint8_t, glm::vec2>>> velocitiesQueue;
};
} // namespace UsArMirror
