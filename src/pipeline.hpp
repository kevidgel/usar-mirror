
#pragma once

#include <openpose/headers.hpp>

namespace UsArMirror {
class Pipeline {
  public:
    Pipeline();

    void start();

  private:
    void configureWrapper(op::Wrapper &op_wrapper);

    op::Wrapper op_wrapper;
};
} // namespace UsArMirror