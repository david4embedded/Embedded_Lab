#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "serial_port.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdlib>
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

namespace
{

using SampleWindow = std::array<float, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE>;
using AccelerationSample = std::array<float, EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME>;

volatile std::sig_atomic_t stop_requested = 0;

void handle_interrupt(int)
{
    stop_requested = 1;
}

class InterruptHandlerGuard
{
public:
    InterruptHandlerGuard()
        : previous_handler_(std::signal(SIGINT, handle_interrupt))
    {
        if (previous_handler_ == SIG_ERR)
        {
            throw std::runtime_error("Could not register Ctrl+C handler");
        }
    }

    ~InterruptHandlerGuard()
    {
        std::signal(SIGINT, previous_handler_);
    }

    InterruptHandlerGuard(const InterruptHandlerGuard &) = delete;
    InterruptHandlerGuard &operator=(const InterruptHandlerGuard &) = delete;

private:
    using SignalHandler = void (*)(int);
    SignalHandler previous_handler_;
};

std::string trim(const std::string &value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
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
    while (std::getline(stream, field, ','))
    {
        fields.push_back(trim(field));
    }
    if (!line.empty() && line.back() == ',')
    {
        fields.emplace_back();
    }
    return fields;
}

bool parse_number(const std::string &text, float &value)
{
    if (text.empty())
    {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        !std::isfinite(parsed))
    {
        return false;
    }
    value = parsed;
    return true;
}

bool parse_imu_stream_line(
    const std::string &line,
    AccelerationSample &sample)
{
    const size_t first_separator = line.find('|');
    if (first_separator == std::string::npos)
    {
        return false;
    }
    const size_t second_separator = line.find('|', first_separator + 1);
    if (second_separator == std::string::npos)
    {
        return false;
    }

    std::istringstream timestamp_stream(line.substr(0, first_separator));
    std::uint64_t timestamp = 0;
    if (!(timestamp_stream >> timestamp))
    {
        return false;
    }
    (void)timestamp;

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

size_t find_column(const std::vector<std::string> &header, const std::string &name)
{
    for (size_t index = 0; index < header.size(); ++index)
    {
        if (header[index] == name)
        {
            return index;
        }
    }
    return header.size();
}

bool read_csv(const std::string &path, std::vector<std::array<float, 3>> &samples)
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
    const auto header = split_csv_row(line);
    const size_t ax_column = find_column(header, "ax");
    const size_t ay_column = find_column(header, "ay");
    const size_t az_column = find_column(header, "az");
    if (ax_column == header.size() || ay_column == header.size() ||
        az_column == header.size())
    {
        std::cerr << "ERROR: CSV header must contain ax, ay, and az columns.\n";
        return false;
    }

    size_t line_number = 1;
    while (std::getline(input, line))
    {
        ++line_number;
        if (trim(line).empty())
        {
            continue;
        }
        const auto fields = split_csv_row(line);
        const size_t max_column = std::max(ax_column, std::max(ay_column, az_column));
        if (fields.size() <= max_column)
        {
            std::cerr << "ERROR: Missing acceleration value on CSV line "
                      << line_number << ".\n";
            return false;
        }

        std::array<float, 3> sample{};
        if (!parse_number(fields[ax_column], sample[0]) ||
            !parse_number(fields[ay_column], sample[1]) ||
            !parse_number(fields[az_column], sample[2]))
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

std::string display_label(const char *model_label)
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

bool classify_window(const SampleWindow &window, size_t window_number)
{
    signal_t signal{};
    signal.total_length = window.size();
    signal.get_data = [&window](size_t offset, size_t length, float *output)
    {
        if (output == nullptr || offset > window.size() ||
            length > window.size() - offset)
        {
            return -1;
        }
        for (size_t index = 0; index < length; ++index)
        {
            output[index] = window[offset + index];
        }
        return 0;
    };

    ei_impulse_result_t result{};
    const EI_IMPULSE_ERROR error = run_classifier(&signal, &result, false);
    if (error != EI_IMPULSE_OK)
    {
        std::cerr << "ERROR: Inference failed for window " << window_number
                  << " with error code " << static_cast<int>(error) << ".\n";
        return false;
    }

    const auto best = std::max_element(
        std::begin(result.classification),
        std::end(result.classification),
        [](const ei_impulse_result_classification_t &left,
           const ei_impulse_result_classification_t &right)
        {
            return left.value < right.value;
        });

    std::cout << "window " << std::setw(4) << window_number
              << "  prediction=" << display_label(best->label)
              << "  confidence=" << std::fixed << std::setprecision(3)
              << best->value;
    for (size_t index = 0; index < EI_CLASSIFIER_LABEL_COUNT; ++index)
    {
        std::cout << "  " << display_label(result.classification[index].label) << '='
                  << std::fixed << std::setprecision(3)
                  << result.classification[index].value;
    }
    std::cout << '\n';
    return true;
}

class ImuStreamStopGuard
{
public:
    explicit ImuStreamStopGuard(platform::SerialPort &serial_port)
        : serial_port_(serial_port), active_(true)
    {
    }

    ~ImuStreamStopGuard()
    {
        if (active_)
        {
            try
            {
                stop();
            }
            catch (const std::exception &error)
            {
                std::cerr << "WARNING: Could not stop board IMU stream: "
                          << error.what() << '\n';
            }
        }
    }

    void stop()
    {
        if (active_)
        {
            serial_port_.write_all(std::string(1, '\x1b'));
            active_ = false;
        }
    }

private:
    platform::SerialPort &serial_port_;
    bool active_;
};

int run_live(const std::string &port_name, std::uint32_t baud_rate)
{
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3)
    {
        throw std::runtime_error("Live IMU input requires a three-axis model");
    }

    platform::SerialPort serial_port(port_name, baud_rate);
    stop_requested = 0;
    InterruptHandlerGuard interrupt_guard;
    ImuStreamStopGuard stream_guard(serial_port);

    std::cout << "Opened " << port_name << " at " << baud_rate << " baud.\n"
              << "Starting board IMU stream every " << EI_CLASSIFIER_INTERVAL_MS
              << " ms. Press Ctrl+C to stop.\n";
    serial_port.write_all(
        "imu stream " + std::to_string(EI_CLASSIFIER_INTERVAL_MS) + "\r");

    std::deque<AccelerationSample> recent_samples;
    constexpr size_t samples_per_window = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    constexpr size_t inference_step = EI_CLASSIFIER_SLICE_SIZE > 0
        ? EI_CLASSIFIER_SLICE_SIZE
        : 1;
    size_t samples_since_inference = 0;
    size_t window_number = 0;
    std::string line;

    while (!stop_requested)
    {
        if (!serial_port.read_line(line, 100))
        {
            continue;
        }

        AccelerationSample sample{};
        if (!parse_imu_stream_line(line, sample))
        {
            continue;
        }

        recent_samples.push_back(sample);
        if (recent_samples.size() > samples_per_window)
        {
            recent_samples.pop_front();
        }
        if (recent_samples.size() < samples_per_window)
        {
            continue;
        }

        if (window_number == 0 || ++samples_since_inference >= inference_step)
        {
            SampleWindow window{};
            size_t sample_index = 0;
            for (const auto &window_sample : recent_samples)
            {
                for (size_t axis = 0; axis < window_sample.size(); ++axis)
                {
                    window[sample_index * window_sample.size() + axis]
                        = window_sample[axis];
                }
                ++sample_index;
            }

            ++window_number;
            if (!classify_window(window, window_number))
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

bool parse_baud_rate(const char *text, std::uint32_t &baud_rate)
{
    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed == 0 ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    baud_rate = static_cast<std::uint32_t>(parsed);
    return true;
}

void print_usage()
{
    std::cerr << "Usage:\n"
              << "  tinyml_gesture_test <imu.csv>\n"
              << "  tinyml_gesture_test --live <COM port> [--baud <rate>]\n"
              << "CSV input columns: timestamp, ax, ay, az\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--live")
    {
        std::uint32_t baud_rate = 115200;
        if (argc != 3 && argc != 5)
        {
            print_usage();
            return 2;
        }
        if (argc == 5 &&
            (std::string(argv[3]) != "--baud" ||
             !parse_baud_rate(argv[4], baud_rate)))
        {
            std::cerr << "ERROR: Baud rate must be a positive 32-bit integer.\n";
            return 2;
        }
        try
        {
            return run_live(argv[2], baud_rate);
        }
        catch (const std::exception &error)
        {
            std::cerr << "ERROR: " << error.what() << '\n';
            return 1;
        }
    }

    if (argc != 2)
    {
        print_usage();
        return 2;
    }

    std::vector<std::array<float, 3>> samples;
    if (!read_csv(argv[1], samples))
    {
        return 1;
    }

    constexpr size_t samples_per_window = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    const size_t complete_windows = samples.size() / samples_per_window;
    const size_t trailing_samples = samples.size() % samples_per_window;
    if (complete_windows == 0)
    {
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
    if (trailing_samples != 0)
    {
        std::cout << "Note: ignoring " << trailing_samples
                  << " trailing sample(s) that do not fill a window.\n";
    }

    for (size_t window_index = 0; window_index < complete_windows; ++window_index)
    {
        SampleWindow window{};
        for (size_t sample_index = 0; sample_index < samples_per_window; ++sample_index)
        {
            const auto &sample = samples[window_index * samples_per_window + sample_index];
            for (size_t axis = 0; axis < sample.size(); ++axis)
            {
                window[sample_index * sample.size() + axis] = sample[axis];
            }
        }
        if (!classify_window(window, window_index + 1))
        {
            return 1;
        }
    }

    return 0;
}
