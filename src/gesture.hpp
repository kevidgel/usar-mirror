#pragma once

#include "camera.hpp"
#include "common.hpp"

#include <glm/vec2.hpp>
#include <openpose/headers.hpp>
#include <optional>

namespace UsArMirror {
/**
 * Backend for Gesture Control.
 * Handles everything from input to keypoint extraction.
 */
class GestureControlPipeline {
  public:
    explicit GestureControlPipeline(const std::shared_ptr<State>& state, const std::shared_ptr<CameraInput> &camera);
    ~GestureControlPipeline();

    void render();
    glm::vec2 getHand(bool isLeft);
    RWDeque<InputEvent> inputEventQueue;

  private:
    using DatumsPtr = std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>;
    RWDeque<std::map<uint8_t, glm::vec2>> keypointQueue;
    std::map<uint8_t, glm::vec2> velocities;
    op::Wrapper opWrapper;
    mutable std::shared_ptr<State> state;
    std::thread captureThread;
    std::shared_ptr<CameraInput> camera;
    bool running;

    static std::optional<std::map<uint8_t, glm::vec2>> getKeypoints(const DatumsPtr &datumsPtr);
    glm::vec2 estimateVelocity(const std::vector<std::pair<std::chrono::system_clock::time_point, glm::vec2>> &measurement) const;
    void processInputs();
    void configureWrapper();
    void captureLoop();
};
} // namespace UsArMirror