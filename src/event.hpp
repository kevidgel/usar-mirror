#pragma once

#include <string>

namespace UsArMirror {
/// Input events
enum class InputEvent {
    LEFT_SWIPE,
    RIGHT_SWIPE,
    DOWN_SWIPE,
    UP_SWIPE
};

inline std::string toString(InputEvent event) {
    switch (event) {
    case InputEvent::LEFT_SWIPE:
        return "LEFT_SWIPE";
    case InputEvent::RIGHT_SWIPE:
        return "RIGHT_SWIPE";
    case InputEvent::DOWN_SWIPE:
        return "DOWN_SWIPE";
    case InputEvent::UP_SWIPE:
        return "UP_SWIPE";
    default:
        return "UNKNOWN_GESTURE";
    }
}
} // namespace UsArMirror
