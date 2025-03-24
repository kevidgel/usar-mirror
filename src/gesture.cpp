#include <glad/glad.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <opencv2/opencv.hpp>
#include <optional>
#include <spdlog/spdlog.h>

#include "gesture.hpp"

#include "kalman.hpp"

namespace UsArMirror {
/// Assign int to arbitrary color
inline glm::vec3 intToRgb(uint i) {
    if (i == 0) {
        return {1.0, 0.0, 0.0};
    }
    const float r = static_cast<float>((i * 81059059) % 256) / 255.f;
    const float g = static_cast<float>((i * 68995967) % 256) / 255.f;
    const float b = static_cast<float>((i * 41394649) % 256) / 255.f;
    return {r, g, b};
}

/**
 * Input wrapper class for openpose wrapper.
 *
 * @param camera Input camera
 */
class PipelineInput : public op::WorkerProducer<std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>> {
  public:
    explicit PipelineInput(const std::shared_ptr<CameraInput> &camera) : camera(camera) {}

    void initializationOnThread() override {}

    std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>> workProducer() override {
        try {
            cv::Mat frame;
            camera->getFrame(frame);
            auto datumsPtr = std::make_shared<std::vector<std::shared_ptr<op::Datum>>>();
            datumsPtr->emplace_back();
            auto &datumPtr = datumsPtr->at(0);
            datumPtr = std::make_shared<op::Datum>();

            datumPtr->cvInputData = OP_CV2OPCONSTMAT(frame);

            if (datumPtr->cvInputData.empty()) {
                datumsPtr = nullptr;
            }

            return datumsPtr;
        } catch (const std::exception &e) {
            this->stop();
            spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
            return nullptr;
        }
    }

  private:
    std::shared_ptr<CameraInput> camera;
};

GestureControlPipeline::GestureControlPipeline(State *state, const std::shared_ptr<CameraInput> &camera)
    : opWrapper{op::ThreadManagerMode::AsynchronousOut}, state(state), camera(camera), running(true) {
    configureWrapper();
    captureThread = std::thread(&GestureControlPipeline::captureLoop, this);
}

GestureControlPipeline::~GestureControlPipeline() {
    running = false;
    if (captureThread.joinable()) {
        captureThread.join();
    }
}

/**
 * Start pipeline. Blocking, so launch in thread.
 */
void GestureControlPipeline::captureLoop() {
    opWrapper.start();

    DatumsPtr datumsPtr{};
    while (running) {
        // Obtain the keypoint samples
        if (opWrapper.waitAndPop(datumsPtr)) {
            auto o = getKeypoints(datumsPtr);
            if (o.has_value()) {
                keypointQueue.push_front(o.value());
            }
        }
    }
}

/**
 * Obtain keypoints from Datum
 */
std::optional<std::map<uint8_t, glm::vec2>> GestureControlPipeline::getKeypoints(const DatumsPtr &datumsPtr) {
    try {
        if (datumsPtr != nullptr && !datumsPtr->empty() && !datumsPtr->at(0)->poseKeypoints.empty()) {
            size_t numPoints = datumsPtr->at(0)->poseKeypoints.getSize().at(1);
            std::map<uint8_t, glm::vec2> points;
            for (size_t i = 0; i < numPoints; i++) {
                const float x = datumsPtr->at(0)->poseKeypoints.at(3 * i);
                const float y = datumsPtr->at(0)->poseKeypoints.at(3 * i + 1);
                // NOTE: We exclude 3 * i + 2, as it is just the confidence of estimate
                if (x > EPS && y > EPS) {
                    glm::vec2 point = {x, y};
                    points.emplace(static_cast<uint8_t>(i), point);
                }
            }
            return points;
        }
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
    return std::nullopt;
}

/**
 * Configure wrapper class for OpenPose
 */
void GestureControlPipeline::configureWrapper() {
    try {
        auto pipelineInput = std::make_shared<PipelineInput>(camera);
        const bool workerInputOnNewThread = true;
        opWrapper.setWorker(op::WorkerType::Input, pipelineInput, workerInputOnNewThread);

        op::WrapperStructPose wrapperStructPose{};
        wrapperStructPose.netInputSize = {576, 320};

        // use defaults (disable output)
        const op::WrapperStructOutput wrapperStructOutput{};

        opWrapper.configure(wrapperStructPose);
        opWrapper.configure(wrapperStructOutput);
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
}

inline float mapToScreen(const float coord, const int cameraExtent) {
    return 1.0f - 2.0f * (coord / (float)cameraExtent);
}

glm::vec2 GestureControlPipeline::estimateVelocity(
    const std::vector<std::pair<std::chrono::system_clock::time_point, glm::vec2>> &measurement) const {

    glm::vec2 initialState(0.f, 0.f);
    glm::mat2 initialP(1.f);

    float dt = (measurement.size() > 1) ? static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                                 measurement[1].first - measurement[0].first)
                                                                 .count()) / 1000000000.f
                                        : 0.03f;
    glm::mat2 A = glm::mat2(1.f, dt, 0.f, 1.f);
    auto Q = glm::mat2(0.1f);
    glm::vec2 H(1.f, 0.f);
    float R = 0.1f;

    KalmanFilter kfX(dt, initialState, initialP, A, Q, H, R);
    KalmanFilter kfY(dt, initialState, initialP, A, Q, H, R);

    for(size_t i = 1; i < measurement.size(); i++) {
        float dt = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(measurement[i].first - measurement[i-1].first).count()) / 1000000000.f;
        glm::mat2 A = glm::mat2(1.f, dt, 0.f, 1.f);

        kfX.setA(A);
        kfY.setA(A);

        float measured_vx = (mapToScreen(measurement[i].second.x, camera->width) - mapToScreen(measurement[i-1].second.x, camera->width)) / dt;
        float measured_vy = (mapToScreen(measurement[i].second.y, camera->height) - mapToScreen(measurement[i-1].second.y, camera->height)) / dt;

        kfX.predict();
        kfX.update(measured_vx);

        kfY.predict();
        kfY.update(measured_vy);

    }

    return {kfX.getState().x,  kfY.getState().x};
}

void GestureControlPipeline::processInputs() {
    // Copy measurements
    std::map<uint8_t, std::vector<std::pair<std::chrono::system_clock::time_point, glm::vec2>>> measurements;
    {
        // Lock queue for read
        std::shared_lock lock(keypointQueue.rwlock);
        for (const auto &poseEntry : keypointQueue.dq) {
            for (const auto &[i, pos] : poseEntry.val) {
                measurements[i].emplace_back(poseEntry.timestamp, pos);
            }
        }
    }

    // Estimate velocities
    for (auto &[i, measurement] : measurements) {
        std::reverse(measurement.begin(), measurement.end());
        velocities[i] = estimateVelocity(measurement);
    }

    // Check right swipe
    // Note this doesn't account for depth
    if (velocities[4].x > 2.f || velocities[7].x > 2.f) {
        auto o = state->inputEventQueue.peek_front();
        if (!o.has_value() || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - o.value().timestamp).count() > 1000) {
            state->inputEventQueue.push_front(InputEvent::RIGHT_SWIPE);
        }
    }

    // Check left swipe
    if (velocities[4].x < -2.f || velocities[7].x <- 2.f) {
        auto o = state->inputEventQueue.peek_front();
        if (!o.has_value() || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - o.value().timestamp).count() > 1000) {
            state->inputEventQueue.push_front(InputEvent::LEFT_SWIPE);
        }
    }

}

void GestureControlPipeline::render() {
    // Evict points every frame
    keypointQueue.evict(std::chrono::milliseconds(1200));
    state->inputEventQueue.evict(std::chrono::milliseconds(5000));

    // Compute velocities
    processInputs();

    // Obtain latest pose for rendering
    auto poseResult = keypointQueue.peek_front();
    std::map<uint8_t, glm::vec2> pose;
    if (poseResult.has_value()) {
        pose = poseResult.value().val;
    }

    if (state->flags.gesture.renderKeypoints) {
        glPointSize(15);
        glEnable(GL_POINT_SMOOTH);
        glBegin(GL_POINTS);

        for (const auto &[i, point] : pose) {
            glm::vec3 color = intToRgb(i);
            glColor3f(color.x, color.y, color.z);
            glVertex2f(mapToScreen(point.x, camera->width), mapToScreen(point.y, camera->height));
        }

        glColor3f(1.0, 1.0, 1.0);
        glEnd();
        glDisable(GL_POINT_SMOOTH);

        for (const auto &[i, point] : pose) {
            glm::vec3 color = intToRgb(i);
            glm::vec2 dir = velocities[i] / 10.f;
            float length = velocities[i].length();
            glLineWidth(length * 100);
            glBegin(GL_LINES);
            glColor3f(color.x, color.y, color.z);
            glVertex2f(mapToScreen(point.x, camera->width), mapToScreen(point.y, camera->height));
            glVertex2f(mapToScreen(point.x, camera->width) + dir.x, mapToScreen(point.y, camera->height) + dir.y);
            glEnd();
        }

        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        for (const auto &[i, point] : pose) {
            const float sx = mapToScreen(point.x, camera->width);
            const float sy = mapToScreen(point.y, camera->height);
            ImVec2 textPos = ImVec2(static_cast<float>(state->viewportWidth) * 0.5f * (sx + 1.f),
                                    static_cast<float>(state->viewportHeight) * 0.5f * (1.f - sy));
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), fmt::format("{}", i).c_str());
        }
    }

    if (state->flags.gesture.renderEyeLevel) {
        glBegin(GL_LINES);
        {
            float avgEyeLevel =
                (mapToScreen(pose[16].y, camera->height) + mapToScreen(pose[15].y, camera->height)) / 2.f;
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex2f(-1, avgEyeLevel);
            glVertex2f(1, avgEyeLevel);
        }
        glEnd();
    }

    // Gesture debug menu
    if (state->flags.gesture.showWindow) {
        std::vector<std::string> events;
        {
            std::shared_lock lock(state->inputEventQueue.rwlock);
            for (const auto& [ts, event] : state->inputEventQueue.dq) {
                events.push_back(toString(event));
            }
        }
        if (ImGui::Begin("Gesture Control Debug")) {
            ImGui::Text("Pose queue size: %d", keypointQueue.size());
            ImGui::Checkbox("Render Keypoints", &state->flags.gesture.renderKeypoints);
            ImGui::Checkbox("Render Eye Level", &state->flags.gesture.renderEyeLevel);

            for (const auto& event : events) {
                ImGui::Text("%s", event.c_str());
            }
        }
        ImGui::End();
    }
}
} // namespace UsArMirror
