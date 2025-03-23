#pragma once

#include "camera.hpp"

#include <glm/vec2.hpp>
#include <openpose/headers.hpp>
#include <optional>

#include "common.hpp"

namespace UsArMirror {
/**
 * Backend for Gesture Control.
 * Handles everything from input to keypoint extraction.
 */
class GestureControlPipeline {
  public:
    explicit GestureControlPipeline(State *state, const std::shared_ptr<CameraInput> &camera);
    ~GestureControlPipeline();

    void render();

  private:
    using DatumsPtr = std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>;
    RWDeque<std::vector<std::pair<uint8_t, glm::vec2>>> keypointQueue;
    op::Wrapper opWrapper;
    State *state;
    std::thread captureThread;
    std::shared_ptr<CameraInput> camera;
    bool running;

    static std::optional<std::vector<std::pair<uint8_t, glm::vec2>>> getKeypoints(const DatumsPtr &datumsPtr);
    void configureWrapper();
    void captureLoop();
};
} // namespace UsArMirror