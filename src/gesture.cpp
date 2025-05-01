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
    explicit PipelineInput(const std::shared_ptr<DepthCameraInput> &camera) : camera(camera) {}

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
    std::shared_ptr<DepthCameraInput> camera;
};

GestureControlPipeline::GestureControlPipeline(const std::shared_ptr<State> &state,
                                               const std::shared_ptr<DepthCameraInput> &camera)
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

// /**
//  * Start pipeline. Blocking, so launch in thread.
//  */
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
        // Compute velocities
        // processInputs();
    }
}

// /**
//  * Obtain keypoints from Datum
//  */
std::optional<std::map<uint8_t, glm::vec2>> GestureControlPipeline::getKeypoints(const DatumsPtr &datumsPtr) {
    try {
        std::map<uint8_t, glm::vec2> points;
        if (datumsPtr != nullptr && !datumsPtr->empty() && !datumsPtr->at(0)->poseKeypoints.empty()) {
            size_t numPoints = datumsPtr->at(0)->poseKeypoints.getSize().at(1);
            for (size_t i = 0; i < numPoints; i++) {
                const float x = datumsPtr->at(0)->poseKeypoints.at(3 * i);
                const float y = datumsPtr->at(0)->poseKeypoints.at(3 * i + 1);
                // NOTE: We exclude 3 * i + 2, as it is just the confidence of estimate
                if (x > EPS && y > EPS) {
                    glm::vec2 point = {x, y};
                    points.emplace(static_cast<uint8_t>(i), point);
                }
            }
        }
        return points;
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
    return std::nullopt;
}

// /**
//  * Configure wrapper class for OpenPose
//  */
void GestureControlPipeline::configureWrapper() {
    try {
        auto pipelineInput = std::make_shared<PipelineInput>(camera);
        const bool workerInputOnNewThread = true;
        opWrapper.setWorker(op::WorkerType::Input, pipelineInput, workerInputOnNewThread);

        op::WrapperStructPose wrapperStructPose{};
        wrapperStructPose.netInputSize = {576, 320};

        op::WrapperStructHand wrapperStructHand{};
        wrapperStructHand.enable = true;

        // use defaults (disable output)
        const op::WrapperStructOutput wrapperStructOutput{};

        opWrapper.configure(wrapperStructPose);
        opWrapper.configure(wrapperStructHand);
        opWrapper.configure(wrapperStructOutput);
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
}

inline float mapToScreen(const float coord, const int cameraExtent) {
    return 1.0f - 2.0f * (coord / (float)cameraExtent);
}

inline glm::vec2 poseToImGUICoords(const glm::vec2& pose, const glm::vec2& cameraSize, const glm::vec2& viewportSize) {
    return pose * viewportSize / cameraSize;
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
}

glm::vec2 GestureControlPipeline::getHand(bool isLeft) {
    auto poseResult = keypointQueue.peek_front();
    int camWidth = camera->width;
    int camHeight = camera->height;
    glm::vec2 camSize = {camWidth, camHeight};
    glm::vec2 viewportSize = {state->viewportWidth, state->viewportHeight};
    std::map<uint8_t, glm::vec2> pose;
    if (poseResult.has_value()) {
        pose = poseResult.value().val;
        if (!isLeft && pose.find(4) != pose.end()) {
            return poseToImGUICoords({pose[4].x, pose[4].y}, camSize, viewportSize);
        }
        if (isLeft && pose.find(7) != pose.end()) {
            return poseToImGUICoords({pose[7].x, pose[7].y}, camSize, viewportSize);
        }
    }
    return {-1, -1};
}

void GestureControlPipeline::render() {
    // Evict points every frame
    keypointQueue.evict(std::chrono::milliseconds(1200));
    state->inputEventQueue.evict(std::chrono::milliseconds(5000));

    int camWidth = camera->width;
    int camHeight = camera->height;
    glm::vec2 camSize = {camWidth, camHeight};
    glm::vec2 viewportSize = {state->viewportWidth, state->viewportHeight};
    float scaling = 1.f;

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
            glVertex2f(mapToScreen(point.x, camWidth), mapToScreen(point.y, camHeight));
        }

        glColor3f(1.0, 1.0, 1.0);
        glEnd();
        glDisable(GL_POINT_SMOOTH);

        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        for (const auto &[i, point] : pose) {
            auto impos = poseToImGUICoords({point.x, point.y}, camSize, viewportSize);
            drawList->AddText({impos.x, impos.y}, IM_COL32(255, 255, 255, 255), fmt::format("{}", i).c_str());
        }
    }

    if (state->flags.gesture.renderHandCircles) {
        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        if (pose.find(4) != pose.end()) {
            auto impos = poseToImGUICoords({pose[4].x, pose[4].y}, camSize, viewportSize);
            drawList->AddCircle({impos.x, impos.y}, 75.f, IM_COL32(255, 255, 255, 150), 40, 3);
        }

        if (pose.find(7) != pose.end()) {
            auto impos = poseToImGUICoords({pose[7].x, pose[7].y}, camSize, viewportSize);
            drawList->AddCircle({impos.x, impos.y}, 75.f, IM_COL32(255, 255, 255, 150), 40, 3);
        }
    }

    if (state->flags.gesture.renderEyeLevel) {
        glBegin(GL_LINES);
        {
            float avgEyeLevel = (mapToScreen(pose[16].y, camHeight) + mapToScreen(pose[15].y, camHeight)) / 2.f;
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
            for (const auto &[ts, event] : state->inputEventQueue.dq) {
                events.push_back(toString(event));
            }
        }
        ImGui::Begin("Gesture Control Debug");
        {
            ImGui::Text("Pose queue size: %d", keypointQueue.size());
            ImGui::Checkbox("Render Keypoints", &state->flags.gesture.renderKeypoints);
            ImGui::Checkbox("Render Eye Level", &state->flags.gesture.renderEyeLevel);

            for (const auto &event : events) {
                ImGui::Text("%s", event.c_str());
            }
        }
        ImGui::End();
    }
}
} // namespace UsArMirror
