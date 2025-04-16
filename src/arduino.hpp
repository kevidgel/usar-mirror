#pragma once

#include <memory>
#include <thread>

#include "common.hpp"

namespace UsArMirror {

class Arduino {
public:
    Arduino(const std::shared_ptr<State>& state);
    ~Arduino();

    void serialLoop();
  private:
    bool running;
    std::shared_ptr<State> state;
    std::thread serialThread;
};

} // namespace UsArMirror
