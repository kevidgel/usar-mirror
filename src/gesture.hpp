#pragma once

#include "camera.hpp"
#include "common.hpp"
#include "depth_camera.hpp"

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
    explicit GestureControlPipeline(const std::shared_ptr<State>& state, const std::shared_ptr<DepthCameraInput> &camera);
    ~GestureControlPipeline();

    void render();
    glm::vec2 getHand(bool isLeft);

  private:
    using DatumsPtr = std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>;
    RWDeque<std::map<uint8_t, glm::vec2>> keypointQueue;
    std::map<uint8_t, glm::vec2> velocities;
    op::Wrapper opWrapper;
    mutable std::shared_ptr<State> state;
    std::thread captureThread;
    std::shared_ptr<DepthCameraInput> camera;
    bool running;

    static std::optional<std::map<uint8_t, glm::vec2>> getKeypoints(const DatumsPtr &datumsPtr);
    void processInputs();
    void configureWrapper();
    void captureLoop();
};
} // namespace UsArMirror