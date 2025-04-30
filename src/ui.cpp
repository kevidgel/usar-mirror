#include <fmt/format.h>
#include <fstream>
#include <imgui.h>

#include "ui.hpp"

#include <unistd.h>

namespace UsArMirror {
UserInterface::UserInterface(const std::shared_ptr<State> &state,
                             const std::shared_ptr<GestureControlPipeline> &gesture)
    : state(state), gesture(gesture),
      leftButton(state, ImVec2(300, state->viewportHeight / 2), 150.f, true, false),
      rightButton(state, ImVec2(state->viewportWidth - 300, state->viewportHeight / 2), 150.f, true, true),
      centerButton(state, ImVec2(state->viewportWidth / 2, state->viewportHeight / 2), 400.f, 150.f),
      topButton(state, ImVec2(state->viewportWidth - 300, state->viewportHeight / 2 - 250), 150.f, false, true),
      bottomButton(state, ImVec2(300, state->viewportHeight / 2 - 100), 150.f, false, false) {}

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
        topButton.detectClick(gesture->getHand(true));
        bottomButton.detectClick(gesture->getHand(false));

        rightButton.draw(drawList);
        leftButton.draw(drawList);
        topButton.draw(drawList);
        bottomButton.draw(drawList);

        auto evOpt = state->inputEventQueue.peek_front();
        if (evOpt.has_value()) {
            InputEvent ev = (*evOpt).val;

            if (ev == InputEvent::RIGHT_SWIPE) {
                currentFilterIndex = (currentFilterIndex + 1) % 3;
                state->inputEventQueue.pop_front();
            } else if (ev == InputEvent::LEFT_SWIPE) {
                currentFilterIndex = (currentFilterIndex - 1 + 3) % 3;
                state->inputEventQueue.pop_front();
            }
        }
    }

    drawFilterTabs();
}

void UserInterface::drawFilterTabs() {
    float tabWidth = 150.f;
    float tabHeight = 50.f;
    float spacing = 50.f;
    float y = state->viewportHeight - 100;
    float centerX = state->viewportWidth / 2;
    const char *labels[] = {"Filter 1", "Filter 2", "Filter 3"};

    // Create a transparent, clickable window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(state->viewportWidth, state->viewportHeight));
    ImGui::Begin("FilterTabsOverlay", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);

    for (int i = 0; i < 3; i++) {
        float x = centerX + (i - 1) * (tabWidth + spacing);
        ImVec2 pos = ImVec2(x, y);
        ImVec2 size = ImVec2(tabWidth, tabHeight);
        ImGui::SetCursorScreenPos(pos);

        ImGui::PushID(i);
        bool isSelected = (currentFilterIndex == i);

        ImGui::PushStyleColor(ImGuiCol_Button,
                              isSelected ? IM_COL32(30, 144, 255, 255) : IM_COL32(70, 130, 180, 200));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255));

        if (ImGui::Button(labels[i], size)) {
            currentFilterIndex = i;
        }
        ImGui::PopStyleColor(2);

        if (isSelected) {
            ImVec2 pMin = ImGui::GetItemRectMin();
            ImVec2 pMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(pMin.x, pMax.y + 2), ImVec2(pMax.x, pMax.y + 2),
                                                IM_COL32(0, 150, 255, 255), 3.0f);
        }
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace UsArMirror