/**
 * @file main.cpp
 * @brief Runs gesture classification from CSV files or a live IMU serial stream.
 * @date 2026-11-12
 * @author Sungsu Kim
 */

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "live_guards.hpp"
#include "serial_port.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using SampleWindow = std::array<float, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE>;
using AccelerationSample = std::array<float, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME>;

static std::string MAIN_trim(const std::string &value);
static std::vector<std::string> MAIN_splitCsvRow(const std::string &line);
static bool MAIN_parseNumber(const std::string &text, float &value);
static bool MAIN_parseImuStreamLine(const std::string &line, AccelerationSample &sample);
static std::size_t MAIN_findColumn(
    const std::vector<std::string> &header,
    const std::string &name);
static bool MAIN_readCsv(
    const std::string &path,
    std::vector<std::array<float, 3>> &samples);
static std::string MAIN_displayLabel(const char *model_label);
static bool MAIN_classifyWindow(
    const SampleWindow &window,
    const std::size_t window_number);
static int MAIN_runLive(
    const std::string &port_name,
    const std::uint32_t baud_rate);
static bool MAIN_parseBaudRate(const char *text, std::uint32_t &baud_rate);
static void MAIN_printUsage();

/**
 * @brief Runs CSV or live-stream gesture classification from command-line arguments.
 * @retval 0 when classification completes successfully
 * @retval 1 when input or inference fails
 * @retval 2 when command-line arguments are invalid
 */
int main(int argc, char **argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--live")
    {
        std::uint32_t baud_rate = 115200;
        if (argc != 3 && argc != 5)
        {
            MAIN_printUsage();
            return 2;
        }
        if (argc == 5 &&
            (std::string(argv[3]) != "--baud" ||
             !MAIN_parseBaudRate(argv[4], baud_rate)))
        {
            std::cerr << "ERROR: Baud rate must be a positive 32-bit integer.\n";
            return 2;
        }
        try
        {
            return MAIN_runLive(argv[2], baud_rate);
        }
        catch (const std::exception &error)
        {
            std::cerr << "ERROR: " << error.what() << '\n';
            return 1;
        }
    }

    if (argc != 2)
    {
        MAIN_printUsage();
        return 2;
    }

    std::vector<std::array<float, 3>> samples;
    if (!MAIN_readCsv(argv[1], samples))
    {
        return 1;
    }

    constexpr std::size_t SAMPLES_PER_WINDOW = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    const std::size_t complete_windows = samples.size() / SAMPLES_PER_WINDOW;
    const std::size_t trailing_samples = samples.size() % SAMPLES_PER_WINDOW;
    if (complete_windows == 0)
    {
        std::cerr << "ERROR: Need at least " << SAMPLES_PER_WINDOW
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
    if (trailing_samples != 0)
    {
        std::cout << "Note: ignoring " << trailing_samples
                  << " trailing sample(s) that do not fill a window.\n";
    }

    for (std::size_t window_index = 0; window_index < complete_windows; ++window_index)
    {
        SampleWindow window;
        window.fill(0.0F);
        for (std::size_t sample_index = 0; sample_index < SAMPLES_PER_WINDOW; ++sample_index)
        {
            const auto &sample = samples[window_index * SAMPLES_PER_WINDOW + sample_index];
            for (std::size_t axis = 0; axis < sample.size(); ++axis)
            {
                window[sample_index * sample.size() + axis] = sample[axis];
            }
        }
        if (!MAIN_classifyWindow(window, window_index + 1))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Removes leading and trailing whitespace from a string.
 * @retval trimmed_string The trimmed string
 */
static std::string MAIN_trim(const std::string &value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return std::string();
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

/**
 * @brief Splits a CSV row into trimmed fields.
 * @retval fields The fields found in the row
 */
static std::vector<std::string> MAIN_splitCsvRow(const std::string &line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ','))
    {
        fields.push_back(MAIN_trim(field));
    }
    if (!line.empty() && line.back() == ',')
    {
        fields.emplace_back();
    }
    return fields;
}

/**
 * @brief Parses a finite floating-point value from text.
 * @retval true when a valid number is parsed
 * @retval false when the text is invalid or out of range
 */
static bool MAIN_parseNumber(const std::string &text, float &value)
{
    if (text.empty())
    {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const float result = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        !std::isfinite(result))
    {
        return false;
    }
    value = result;
    return true;
}

/**
 * @brief Parses the timestamp and three acceleration axes from a live IMU line.
 * @retval true when a valid sample is parsed
 * @retval false when the line does not contain a valid sample
 */
static bool MAIN_parseImuStreamLine(const std::string &line, AccelerationSample &sample)
{
    const std::size_t first_separator = line.find('|');
    if (first_separator == std::string::npos)
    {
        return false;
    }
    const std::size_t second_separator = line.find('|', first_separator + 1);
    if (second_separator == std::string::npos)
    {
        return false;
    }

    std::istringstream timestamp_stream(line.substr(0, first_separator));
    [[maybe_unused]] std::uint64_t timestamp = 0;
    if (!(timestamp_stream >> timestamp))
    {
        return false;
    }

    std::istringstream acceleration_stream(
        line.substr(first_separator + 1, second_separator - first_separator - 1));
    if (!(acceleration_stream >> sample[0] >> sample[1] >> sample[2]))
    {
        return false;
    }
    return std::isfinite(sample[0]) &&
        std::isfinite(sample[1]) &&
        std::isfinite(sample[2]);
}

/**
 * @brief Finds the index of a named column in a CSV header.
 * @retval column_index The matching index or header size when absent
 */
static std::size_t MAIN_findColumn(
    const std::vector<std::string> &header,
    const std::string &name)
{
    for (std::size_t index = 0; index < header.size(); ++index)
    {
        if (header[index] == name)
        {
            return index;
        }
    }
    return header.size();
}

/**
 * @brief Reads acceleration samples from a CSV file.
 * @retval true when the file is read successfully
 * @retval false when the file or its acceleration data is invalid
 */
static bool MAIN_readCsv(
    const std::string &path,
    std::vector<std::array<float, 3>> &samples)
{
    std::ifstream input(path);
    if (!input)
    {
        std::cerr << "ERROR: Cannot open CSV file: " << path << '\n';
        return false;
    }

    std::string line;
    if (!std::getline(input, line))
    {
        std::cerr << "ERROR: CSV file is empty.\n";
        return false;
    }
    const auto header = MAIN_splitCsvRow(line);
    const std::size_t ax_column = MAIN_findColumn(header, "ax");
    const std::size_t ay_column = MAIN_findColumn(header, "ay");
    const std::size_t az_column = MAIN_findColumn(header, "az");
    if (ax_column == header.size() || ay_column == header.size() ||
        az_column == header.size())
    {
        std::cerr << "ERROR: CSV header must contain ax, ay, and az columns.\n";
        return false;
    }

    std::size_t line_number = 1;
    while (std::getline(input, line))
    {
        ++line_number;
        if (MAIN_trim(line).empty())
        {
            continue;
        }
        const auto fields = MAIN_splitCsvRow(line);
        const std::size_t max_column = std::max(ax_column, std::max(ay_column, az_column));
        if (fields.size() <= max_column)
        {
            std::cerr << "ERROR: Missing acceleration value on CSV line "
                      << line_number << ".\n";
            return false;
        }

        std::array<float, 3> sample;
        if (!MAIN_parseNumber(fields[ax_column], sample[0]) ||
            !MAIN_parseNumber(fields[ay_column], sample[1]) ||
            !MAIN_parseNumber(fields[az_column], sample[2]))
        {
            std::cerr << "ERROR: Invalid acceleration value on CSV line "
                      << line_number << ".\n";
            return false;
        }
        samples.push_back(sample);
    }

    if (input.bad())
    {
        std::cerr << "ERROR: Failed while reading CSV file: " << path << '\n';
        return false;
    }
    return true;
}

/**
 * @brief Converts exported model labels to concise display labels.
 * @retval display_label The concise or original model label
 */
static std::string MAIN_displayLabel(const char *model_label)
{
    const std::string label(model_label);
    if (label.rfind("acc_gesture_idle_", 0) == 0)
    {
        return "Idle";
    }
    if (label.rfind("acc_gesture_x_", 0) == 0)
    {
        return "Shake X";
    }
    if (label.rfind("acc_gesture_y_", 0) == 0)
    {
        return "Shake Y";
    }
    return label;
}

/**
 * @brief Runs inference on one sample window and prints the class scores.
 * @retval true when inference succeeds
 * @retval false when inference fails
 */
static bool MAIN_classifyWindow(
    const SampleWindow &window,
    const std::size_t window_number)
{
    signal_t signal = signal_t();
    signal.total_length = window.size();
    signal.get_data = [&window](
        const std::size_t offset,
        const std::size_t length,
        float *output) -> int
    {
        if (output == nullptr || offset > window.size() ||
            length > window.size() - offset)
        {
            return -1;
        }
        for (std::size_t index = 0; index < length; ++index)
        {
            output[index] = window[offset + index];
        }
        return 0;
    };

    ei_impulse_result_t classifier_result = ei_impulse_result_t();
    const EI_IMPULSE_ERROR result = run_classifier(&signal, &classifier_result, false);
    if (result != EI_IMPULSE_OK)
    {
        std::cerr << "ERROR: Inference failed for window " << window_number
                  << " with error code " << static_cast<int>(result) << ".\n";
        return false;
    }

    const auto best = std::max_element(
        std::begin(classifier_result.classification),
        std::end(classifier_result.classification),
        [](const ei_impulse_result_classification_t &left,
           const ei_impulse_result_classification_t &right) -> bool
        {
            return left.value < right.value;
        });

    std::cout << "window " << std::setw(4) << window_number
              << "  prediction=" << MAIN_displayLabel(best->label)
              << "  confidence=" << std::fixed << std::setprecision(3)
              << best->value;
    for (std::size_t index = 0; index < EI_CLASSIFIER_LABEL_COUNT; ++index)
    {
        std::cout << "  " << MAIN_displayLabel(
            classifier_result.classification[index].label) << '='
                  << std::fixed << std::setprecision(3)
                  << classifier_result.classification[index].value;
    }
    std::cout << '\n';
    return true;
}

/**
 * @brief Runs continuous gesture classification from a board serial stream.
 * @retval 0 when the stream stops normally
 * @retval 1 when inference fails
 */
static int MAIN_runLive(
    const std::string &port_name,
    const std::uint32_t baud_rate)
{
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3)
    {
        throw std::runtime_error("Live IMU input requires a three-axis model");
    }

    platform::SerialPort serial_port(port_name, baud_rate);
    platform::InterruptHandlerGuard interrupt_guard;
    platform::ImuStreamStopGuard stream_guard(serial_port);

    std::cout << "Opened " << port_name << " at " << baud_rate << " baud.\n"
              << "Starting board IMU stream every " << EI_CLASSIFIER_INTERVAL_MS
              << " ms. Press Ctrl+C to stop.\n";
    serial_port.writeAll(
        "imu stream " + std::to_string(EI_CLASSIFIER_INTERVAL_MS) + "\r");

    std::deque<AccelerationSample> recent_samples;
    constexpr std::size_t SAMPLES_PER_WINDOW = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    constexpr std::size_t INFERENCE_STEP = EI_CLASSIFIER_SLICE_SIZE > 0
        ? EI_CLASSIFIER_SLICE_SIZE
        : 1;
    std::size_t samples_since_inference = 0;
    std::size_t window_number = 0;
    std::string line;

    while (!interrupt_guard.isStopRequested())
    {
        if (!serial_port.readLine(line, 100))
        {
            continue;
        }

        AccelerationSample sample;
        if (!MAIN_parseImuStreamLine(line, sample))
        {
            continue;
        }

        recent_samples.push_back(sample);
        if (recent_samples.size() > SAMPLES_PER_WINDOW)
        {
            recent_samples.pop_front();
        }
        if (recent_samples.size() < SAMPLES_PER_WINDOW)
        {
            continue;
        }

        if (window_number == 0 || ++samples_since_inference >= INFERENCE_STEP)
        {
            SampleWindow window;
            window.fill(0.0F);
            std::size_t sample_index = 0;
            for (const auto &window_sample : recent_samples)
            {
                for (std::size_t axis = 0; axis < window_sample.size(); ++axis)
                {
                    window[sample_index * window_sample.size() + axis]
                        = window_sample[axis];
                }
                ++sample_index;
            }

            ++window_number;
            if (!MAIN_classifyWindow(window, window_number))
            {
                return 1;
            }
            samples_since_inference = 0;
        }
    }

    stream_guard.stop();
    std::cout << "\nIMU stream stopped after " << window_number
              << " inference window(s).\n";
    return 0;
}

/**
 * @brief Parses a positive 32-bit baud rate from command-line text.
 * @retval true when a valid baud rate is parsed
 * @retval false when the text is invalid or out of range
 */
static bool MAIN_parseBaudRate(const char *text, std::uint32_t &baud_rate)
{
    char *end = nullptr;
    errno = 0;
    const unsigned long result = std::strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || result == 0 ||
        result > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    baud_rate = static_cast<std::uint32_t>(result);
    return true;
}

/**
 * @brief Prints the supported command-line forms.
 * @retval None
 */
static void MAIN_printUsage()
{
    std::cerr << "Usage:\n"
              << "  tinyml_gesture_test <imu.csv>\n"
              << "  tinyml_gesture_test --live <COM port> [--baud <rate>]\n"
              << "CSV input columns: timestamp, ax, ay, az\n";
}
