//
// Created by kevidgel on 2/19/25.
//

#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include "pipeline.hpp"

#include <optional>

namespace UsArMirror {
Pipeline::Pipeline(State *state) : opWrapper{op::ThreadManagerMode::Asynchronous}, state(state) {
    configureWrapper();
}

/**
 * Start pipeline. Blocking, so launch in thread.
 */
void Pipeline::start() {
    opWrapper.start();

    DatumsPtr datumsPtr{};
    while (!state->die) {
        // Obtain the keypoint samples
        if (opWrapper.waitAndPop(datumsPtr)) {
            auto o = getKeypoints(datumsPtr);
            if (o.has_value()) {
                state->keypointQueue.push_front(o.value());
            }
        }

        // Compute velocities
        if (state->keypointQueue.peek_front()->timestamp - state->keypointQueue.peek_back()->timestamp >=
            std::chrono::milliseconds(500)) {
            // Ensure enough data to compute velocities
        }
    }
}

std::optional<std::vector<std::pair<uint8_t, glm::vec2>>> Pipeline::getKeypoints(const DatumsPtr &datumsPtr) {
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
void Pipeline::configureWrapper() {
    try {
        op::WrapperStructInput wrapperStructInput{
            op::ProducerType::Webcam,
            "-1",
            0,
            2,
            std::numeric_limits<unsigned long long>::max(),
            false,
            false,
            0,
            false, // doesn't apply but whatever
            op::Point{1920, 1080},
            "",
            false,
            -2
        };

        op::WrapperStructPose wrapperStructPose{};
        wrapperStructPose.netInputSize = {576, 320};

        // use defaults (disable output)
        op::WrapperStructOutput wrapperStructOutput{};

        opWrapper.configure(wrapperStructInput);
        opWrapper.configure(wrapperStructPose);
        opWrapper.configure(wrapperStructOutput);
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
}
} // namespace UsArMirror
