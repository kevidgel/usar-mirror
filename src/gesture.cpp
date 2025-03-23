#include <glad/glad.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <opencv2/opencv.hpp>
#include <optional>
#include <spdlog/spdlog.h>

#include "gesture.hpp"

namespace UsArMirror {
/// Assign int to arbitrary color
inline glm::vec3 intToRgb(uint i) {
    const float r = static_cast<float>((i * 81059059) % 256) / 255.f;
    const float g = static_cast<float>((i * 68995967) % 256) / 255.f;
    const float b = static_cast<float>((i * 41394649) % 256) / 255.f;
    return {r, g, b};
}

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

std::optional<std::vector<std::pair<uint8_t, glm::vec2>>>
GestureControlPipeline::getKeypoints(const DatumsPtr &datumsPtr) {
    try {
        if (datumsPtr != nullptr && !datumsPtr->empty() && !datumsPtr->at(0)->poseKeypoints.empty()) {
            size_t numPoints = datumsPtr->at(0)->poseKeypoints.getSize().at(1);
            std::vector<std::pair<uint8_t, glm::vec2>> points;
            for (size_t i = 0; i < numPoints; i++) {
                const float x = datumsPtr->at(0)->poseKeypoints.at(3 * i);
                const float y = datumsPtr->at(0)->poseKeypoints.at(3 * i + 1);
                if (x > EPS && y > EPS) {
                    points.push_back({
                        static_cast<uint8_t>(i), {x, y}
                    });
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

void GestureControlPipeline::render() {
    std::vector<std::pair<uint8_t, glm::vec2>> pose;

    // Map to screen space
    auto o = keypointQueue.peek_front();
    keypointQueue.evict(std::chrono::milliseconds(1200));
    if (o.has_value()) {
        for (const auto &[i, point] : o.value().val) {
            float screenX = 1.0f - 2.0f * (point.x / (float) camera->width);
            float screenY = 1.0f - 2.0f * (point.y / (float) camera->height);
            glm::vec2 screenCoords = {screenX, screenY};
            pose.emplace_back(i, screenCoords);
        }
    }

    if (state->flags.gesture.renderKeypoints) {
        glPointSize(20.0);
        glBegin(GL_POINTS);

        for (const auto &[i, point] : pose) {
            glm::vec3 color = intToRgb(i);
            glColor3f(color.x, color.y, color.z);
            glVertex2f(point.x, point.y);
        }

        glColor3f(1.0, 1.0, 1.0);
        glEnd();
    }

    // Gesture debug menu
    if (state->flags.gesture.showWindow) {
        if (ImGui::Begin("Gesture Control Debug")) {
            ImGui::Checkbox("Render Keypoints", &state->flags.gesture.renderKeypoints);
            for (const auto &[i, point] : pose) {
                ImGui::Text("(%d): (%f, %f)", i, point.x, point.y);
            }
        }
        ImGui::End();
    }
}
} // namespace UsArMirror
