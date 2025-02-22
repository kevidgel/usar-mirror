//
// Created by kevidgel on 2/19/25.
//

#include <spdlog/spdlog.h>

#include "pipeline.hpp"

namespace UsArMirror {
Pipeline::Pipeline() : op_wrapper{op::ThreadManagerMode::Asynchronous} {
    configureWrapper(op_wrapper);
}

/**
 * Non-blocking start
 */
void Pipeline::start() {
    op_wrapper.start();
}

/**
 * Configure wrapper class for OpenPose
 */
void Pipeline::configureWrapper(op::Wrapper &op_wrapper) {
    try {
        op::ProducerType producer_type = op::ProducerType::Webcam;
        // default webcam
        op::String producer_string = "-1";

        const op::WrapperStructInput wrapper_struct_input {
            producer_type,
            producer_string,
            0,
            2,
            std::numeric_limits<unsigned long long>::max(),
            false,
            false,
            0,
            false, // doesn't apply but whatever
            op::Point<int> {1920, 1080},
            "",
            false,
            -2
        };

        op_wrapper.configure(wrapper_struct_input);
    } catch (const std::exception &e) {
        spdlog::error("{} at {}:{}:{}", e.what(), __FILE__, __FUNCTION__, __LINE__);
    }
}
} // namespace UsArMirror
