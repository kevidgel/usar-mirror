#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

#include "common.hpp"
#include "gesture.hpp"


namespace UsArMirror {

/// Clickable arrow button element
class ArrowButton {
  public:
    ArrowButton(const std::shared_ptr<State> &state, ImVec2 pos, float size, bool isRight)
        : state(state), size(size), isRight(isRight) {
        p1 = pos;
        p2 = ImVec2(p1.x + (isRight ? size : -size), p1.y + size * 0.5f);
        p3 = ImVec2(p1.x, p1.y + size);
    }

    void draw(ImDrawList *drawList) {
        // Draw arrow
        drawList->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 255, 255, 100 + 155 * (progress > 0 ? 1 : 0)));

        // Check for inactivity and reset progress if needed
        const float inactivityThreshold = 1.f; // seconds
        double currentTime = ImGui::GetTime();
        if (currentTime - lastInteractionTime > inactivityThreshold) {
            progress = 0.f;
            return;
        }

        // Buffer after progress completion
        const float bufferDuration = 1.0f;
        if (progress >= 1.0f && bufferStartTime < 0.0f) {
            bufferStartTime = currentTime;
        }

        if (bufferStartTime >= 0.0f && currentTime - bufferStartTime >= bufferDuration) {
            progress = 0.f;
            bufferStartTime = -1.0f;
        }

        // Draw progress circle
        if (progress > 0.f) {
            ImVec2 center = ImVec2((p1.x + p2.x + p3.x) / 3.f, (p1.y + p2.y + p3.y) / 3.f);
            drawList->AddCircle(ImVec2(center.x, center.y), size / 5, IM_COL32(100, 100, 100, 255), 64, 5.0f);

            float start_angle = 0.0f;
            float end_angle = progress * 2.0f * M_PIf; // Convert progress to radians
            drawList->PathArcTo(center, size / 5, start_angle, end_angle, 64);
            if (progress >= 1.f) {
                drawList->PathStroke(IM_COL32(0, 0, 255, 150), false, 10.0f);
            } else {
                drawList->PathStroke(IM_COL32(0, 255, 0, 150), false, 10.0f);
            }

        }
    }

    void detectClick(glm::vec2 pos) {
        // Map to imgui coordinates
        glm::vec2 imguiCoords = {(state->viewportWidth - pos.x) * state->viewportScaling,
                                 pos.y * state->viewportScaling};

        glm::vec2 pp1 = {p1.x, p1.y};
        glm::vec2 pp2 = {p2.x, p2.y};
        glm::vec2 pp3 = {p3.x, p3.y};

        glm::vec2 d1 = imguiCoords - pp1;
        glm::vec2 d2 = imguiCoords - pp2;
        glm::vec2 d3 = imguiCoords - pp3;
        if (length(d1) < 75 || length(d2) < 75 || length(d3) < 75) {
            progress += 1.2f * ImGui::GetIO().DeltaTime;
            lastInteractionTime = ImGui::GetTime();
        }

        if (progress > 1.0f) {
            progress = 1.f;
        }
    }

  private:
    std::shared_ptr<State> state;
    ImVec2 p1, p2, p3;
    bool isRight;
    ImU32 color = IM_COL32(255, 255, 255, 255);
    float size;
    float progress = 0.0f;
    double lastInteractionTime;
    double bufferStartTime;
};

class UserInterface {
  public:
    explicit UserInterface(const std::shared_ptr<State> &state,
                           const std::shared_ptr<GestureControlPipeline> &gesture);

    void render();

  private:
    std::shared_ptr<State> state;
    std::shared_ptr<GestureControlPipeline> gesture;
    ArrowButton leftButton;
    ArrowButton rightButton;

    void menuBar();
};
} // namespace UsArMirror