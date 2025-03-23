#include <imgui.h>

#include "ui.hpp"

#include <fmt/format.h>

namespace UsArMirror {
UserInterface::UserInterface(State *state) : state(state) {}

void UserInterface::render() {
    // Menu Bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Gesture Control")) {
                state->flags.gesture.showWindow = !state->flags.gesture.showWindow;
            }
            ImGui::EndMenu();
        }
        auto fps = fmt::format("{:.1f} FPS", ImGui::GetIO().Framerate);
        float window_width = ImGui::GetWindowWidth();;
        float text_width = ImGui::CalcTextSize(fps.c_str()).x;
        ImGui::SameLine(window_width - 10 - text_width);
        ImGui::Text("%s", fps.c_str());
        ImGui::EndMainMenuBar();
    }
}

} // namespace UsArMirror