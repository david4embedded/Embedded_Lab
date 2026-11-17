#pragma once

#include "model-parameters/model_metadata.h"

#include <cstddef>

namespace GestureClassifier
{

constexpr std::size_t kSampleCount = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
constexpr std::size_t kAxisCount = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
constexpr std::size_t kValueCount = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
constexpr std::size_t kClassCount = EI_CLASSIFIER_LABEL_COUNT;
constexpr std::size_t kInferenceStep = EI_CLASSIFIER_SLICE_SIZE > 0
    ? EI_CLASSIFIER_SLICE_SIZE
    : 1;

struct Result
{
    const char* prediction;
    float confidence;
    const char* labels[kClassCount];
    float scores[kClassCount];
};

int Classify(const float* interleaved_accel, std::size_t value_count, Result& result);

} // namespace GestureClassifier
