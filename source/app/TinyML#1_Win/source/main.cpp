#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

using SampleWindow = std::array<float, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE>;

std::string trim(const std::string &value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_csv_row(const std::string &line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(trim(field));
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

bool parse_number(const std::string &text, float &value)
{
    if (text.empty()) {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

size_t find_column(const std::vector<std::string> &header, const std::string &name)
{
    for (size_t index = 0; index < header.size(); ++index) {
        if (header[index] == name) {
            return index;
        }
    }
    return header.size();
}

bool read_csv(const std::string &path, std::vector<std::array<float, 3>> &samples)
{
    std::ifstream input(path);
    if (!input) {
        std::cerr << "ERROR: Cannot open CSV file: " << path << '\n';
        return false;
    }

    std::string line;
    if (!std::getline(input, line)) {
        std::cerr << "ERROR: CSV file is empty.\n";
        return false;
    }
    const auto header = split_csv_row(line);
    const size_t ax_column = find_column(header, "ax");
    const size_t ay_column = find_column(header, "ay");
    const size_t az_column = find_column(header, "az");
    if (ax_column == header.size() || ay_column == header.size() ||
        az_column == header.size()) {
        std::cerr << "ERROR: CSV header must contain ax, ay, and az columns.\n";
        return false;
    }

    size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }
        const auto fields = split_csv_row(line);
        const size_t max_column = std::max(ax_column, std::max(ay_column, az_column));
        if (fields.size() <= max_column) {
            std::cerr << "ERROR: Missing acceleration value on CSV line "
                      << line_number << ".\n";
            return false;
        }

        std::array<float, 3> sample{};
        if (!parse_number(fields[ax_column], sample[0]) ||
            !parse_number(fields[ay_column], sample[1]) ||
            !parse_number(fields[az_column], sample[2])) {
            std::cerr << "ERROR: Invalid acceleration value on CSV line "
                      << line_number << ".\n";
            return false;
        }
        samples.push_back(sample);
    }

    if (input.bad()) {
        std::cerr << "ERROR: Failed while reading CSV file: " << path << '\n';
        return false;
    }
    return true;
}

bool classify_window(const SampleWindow &window, size_t window_number)
{
    signal_t signal{};
    signal.total_length = window.size();
    signal.get_data = [&window](size_t offset, size_t length, float *output) {
        if (output == nullptr || offset > window.size() ||
            length > window.size() - offset) {
            return -1;
        }
        for (size_t index = 0; index < length; ++index) {
            output[index] = window[offset + index];
        }
        return 0;
    };

    ei_impulse_result_t result{};
    const EI_IMPULSE_ERROR error = run_classifier(&signal, &result, false);
    if (error != EI_IMPULSE_OK) {
        std::cerr << "ERROR: Inference failed for window " << window_number
                  << " with error code " << static_cast<int>(error) << ".\n";
        return false;
    }

    const auto best = std::max_element(
        std::begin(result.classification),
        std::end(result.classification),
        [](const ei_impulse_result_classification_t &left,
           const ei_impulse_result_classification_t &right) {
            return left.value < right.value;
        });

    std::cout << "window " << std::setw(4) << window_number
              << "  prediction=" << best->label
              << "  confidence=" << std::fixed << std::setprecision(3)
              << best->value;
    for (size_t index = 0; index < EI_CLASSIFIER_LABEL_COUNT; ++index) {
        std::cout << "  " << result.classification[index].label << '='
                  << std::fixed << std::setprecision(3)
                  << result.classification[index].value;
    }
    std::cout << '\n';
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: tinyml_gesture_test <imu.csv>\n";
        std::cerr << "Expected CSV columns include timestamp, ax, ay, az.\n";
        return 2;
    }

    std::vector<std::array<float, 3>> samples;
    if (!read_csv(argv[1], samples)) {
        return 1;
    }

    constexpr size_t samples_per_window = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    const size_t complete_windows = samples.size() / samples_per_window;
    const size_t trailing_samples = samples.size() % samples_per_window;
    if (complete_windows == 0) {
        std::cerr << "ERROR: Need at least " << samples_per_window
                  << " samples; CSV contains " << samples.size() << ".\n";
        return 1;
    }

    std::cout << "Model: " << EI_CLASSIFIER_PROJECT_NAME
              << " (deployment " << EI_CLASSIFIER_PROJECT_DEPLOY_VERSION << ")\n"
              << "Input: " << EI_CLASSIFIER_FREQUENCY << " Hz, "
              << EI_CLASSIFIER_RAW_SAMPLE_COUNT << " samples/window, "
              << EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME << " axes (ax, ay, az)\n"
              << "Loaded " << samples.size() << " rows; classifying "
              << complete_windows << " non-overlapping window(s).\n";
    if (trailing_samples != 0) {
        std::cout << "Note: ignoring " << trailing_samples
                  << " trailing sample(s) that do not fill a window.\n";
    }

    for (size_t window_index = 0; window_index < complete_windows; ++window_index) {
        SampleWindow window{};
        for (size_t sample_index = 0; sample_index < samples_per_window; ++sample_index) {
            const auto &sample = samples[window_index * samples_per_window + sample_index];
            for (size_t axis = 0; axis < sample.size(); ++axis) {
                window[sample_index * sample.size() + axis] = sample[axis];
            }
        }
        if (!classify_window(window, window_index + 1)) {
            return 1;
        }
    }

    return 0;
}
