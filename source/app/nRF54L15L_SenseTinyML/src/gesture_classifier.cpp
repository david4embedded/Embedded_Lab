#include "gesture_classifier.hpp"

/**
 * @file gesture_classifier.cpp
 * @brief Edge Impulse gesture classification implementation.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include <algorithm>
#include <cstring>
#include <iterator>

static const char* displayLabel(const char* label);

/**
 * @brief Convert generated Edge Impulse labels to display labels.
 * @retval Pointer to the display label
 */
static const char* displayLabel(const char* label)
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

/**
 * @brief Classify one interleaved accelerometer window.
 * @retval 0 on success, negative errno or classifier error otherwise
 */
int GestureClassifier::Classify(const float* interleavedAccel, std::size_t valueCount, Result& result)
{
    if (interleavedAccel == nullptr || valueCount != VALUE_COUNT)
    {
        return -1;
    }

    signal_t signal = {};
    signal.total_length = valueCount;
    signal.get_data = [interleavedAccel, valueCount](
        std::size_t offset,
        std::size_t length,
        float* output) -> int
    {
        if (output == nullptr || offset > valueCount || length > valueCount - offset)
        {
            return -1;
        }
        std::copy(interleavedAccel + offset, interleavedAccel + offset + length, output);
        return 0;
    };

    ei_impulse_result_t inference = {};
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

    result.m_prediction = displayLabel(best->label);
    result.m_confidence = best->value;
    for (std::size_t index = 0; index < CLASS_COUNT; ++index)
    {
        result.m_labels[index] = displayLabel(inference.classification[index].label);
        result.m_scores[index] = inference.classification[index].value;
    }

    return 0;
}
