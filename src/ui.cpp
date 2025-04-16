#include <fmt/format.h>
#include <fstream>
#include <imgui.h>

#include "ui.hpp"

#include <unistd.h>

namespace UsArMirror {
UserInterface::UserInterface(const std::shared_ptr<State> &state,
                             const std::shared_ptr<GestureControlPipeline> &gesture)
    : state(state), gesture(gesture), leftButton(state, ImVec2(300, state->viewportHeight - 300), 150.f, false),
      rightButton(state, ImVec2(state->viewportWidth - 300, state->viewportHeight - 300), 150.f, true) {}

void UserInterface::menuBar() {
    // Menu Bar
    if (state->flags.showDebug && ImGui::BeginMainMenuBar()) {
        // Debug menu
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("General")) {
                state->flags.general.showWindow = !state->flags.general.showWindow;
            }
            // Gesture control menu
            if (ImGui::MenuItem("Gesture Control")) {
                state->flags.gesture.showWindow = !state->flags.gesture.showWindow;
            }
            ImGui::EndMenu();
        }

        // FPS counter
        {
            auto fps = fmt::format("{:.1f} FPS", ImGui::GetIO().Framerate);
            float window_width = ImGui::GetWindowWidth();
            ;
            float text_width = ImGui::CalcTextSize(fps.c_str()).x;
            ImGui::SameLine(window_width - 10 - text_width);
            ImGui::Text("%s", fps.c_str());
        }
        ImGui::EndMainMenuBar();
    }

    // General menu
    if (state->flags.general.showWindow) {
        ImGui::Begin("General");
        std::ifstream statm("/proc/self/statm");
        size_t size, resident, share, text, lib, data, dt;
        statm >> size >> resident >> share >> text >> lib >> data >> dt;
        ImGui::Text("MEM: %f MB", static_cast<float>(resident * sysconf(_SC_PAGESIZE)) / (1024.f * 1024.f));
        ImGui::End();
    }
}

void UserInterface::render() {
    // Get drawlist and pose
    ImDrawList *drawList = ImGui::GetForegroundDrawList();

    menuBar();

    // LR button clicks and click detection
    {
        leftButton.detectClick(gesture->getHand(true));
        rightButton.detectClick(gesture->getHand(false));
        rightButton.draw(drawList);
        leftButton.draw(drawList);
    }
}

} // namespace UsArMirror