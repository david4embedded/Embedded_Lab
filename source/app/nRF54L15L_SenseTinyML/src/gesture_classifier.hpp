#pragma once

/**
 * @file gesture_classifier.hpp
 * @brief Interface for Edge Impulse gesture classification.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include "model-parameters/model_metadata.h"

#include <cstddef>

namespace GestureClassifier
{
constexpr std::size_t SAMPLE_COUNT = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
constexpr std::size_t AXIS_COUNT = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
constexpr std::size_t VALUE_COUNT = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
constexpr std::size_t CLASS_COUNT = EI_CLASSIFIER_LABEL_COUNT;
constexpr std::size_t INFERENCE_STEP = EI_CLASSIFIER_SLICE_SIZE > 0
    ? EI_CLASSIFIER_SLICE_SIZE
    : 1;

struct Result
{
    const char* m_prediction; //!< Display label selected by the classifier.
    float m_confidence; //!< Confidence of the selected class.
    const char* m_labels[CLASS_COUNT]; //!< Display labels for all classes.
    float m_scores[CLASS_COUNT]; //!< Confidence scores for all classes.
};

int Classify(const float* interleavedAccel, std::size_t valueCount, Result& result);
}
