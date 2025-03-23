#pragma once

#include "common.hpp"

namespace UsArMirror {
class UserInterface {
  public:
    explicit UserInterface(State *state);

    void render();

  private:
    State *state;
};
} // namespace UsArMirror