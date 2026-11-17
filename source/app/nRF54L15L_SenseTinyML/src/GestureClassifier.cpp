#include "GestureClassifier.hpp"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace
{

const char* DisplayLabel(const char* label)
{
    if (std::strncmp(label, "acc_gesture_idle_", sizeof("acc_gesture_idle_") - 1) == 0)
    {
        return "Idle";
    }
    if (std::strncmp(label, "acc_gesture_x_", sizeof("acc_gesture_x_") - 1) == 0)
    {
        return "Shake X";
    }
    if (std::strncmp(label, "acc_gesture_y_", sizeof("acc_gesture_y_") - 1) == 0)
    {
        return "Shake Y";
    }
    return label;
}

} // namespace

int GestureClassifier::Classify(
    const float* interleaved_accel,
    std::size_t value_count,
    Result& result)
{
    if (interleaved_accel == nullptr || value_count != kValueCount)
    {
        return -1;
    }

    signal_t signal{};
    signal.total_length = value_count;
    signal.get_data = [interleaved_accel, value_count](
        std::size_t offset,
        std::size_t length,
        float* output)
    {
        if (output == nullptr || offset > value_count ||
            length > value_count - offset)
        {
            return -1;
        }
        std::copy(interleaved_accel + offset, interleaved_accel + offset + length, output);
        return 0;
    };

    ei_impulse_result_t inference{};
    const EI_IMPULSE_ERROR error = run_classifier(&signal, &inference, false);
    if (error != EI_IMPULSE_OK)
    {
        return static_cast<int>(error);
    }

    const auto best = std::max_element(
        std::begin(inference.classification),
        std::end(inference.classification),
        [](const ei_impulse_result_classification_t& left,
           const ei_impulse_result_classification_t& right)
        {
            return left.value < right.value;
        });

    result.prediction = DisplayLabel(best->label);
    result.confidence = best->value;
    for (std::size_t index = 0; index < kClassCount; ++index)
    {
        result.labels[index] = DisplayLabel(inference.classification[index].label);
        result.scores[index] = inference.classification[index].value;
    }

    return 0;
}
