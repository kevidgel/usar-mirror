
#pragma once

#include <glm/vec2.hpp>
#include <openpose/headers.hpp>
#include <optional>

#include "common.hpp"

namespace UsArMirror {
class Pipeline {
  public:
    explicit Pipeline(State* state);

    void start();

  private:
    using DatumsPtr = std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>;

    void configureWrapper();
    std::optional<std::vector<std::pair<uint8_t, glm::vec2>>> getKeypoints(const DatumsPtr &datumsPtr);

    op::Wrapper opWrapper;
    State *state;
};
} // namespace UsArMirror