#pragma once

#include <cstdint>
#include <string>

namespace platform {

class SerialPort {
public:
    SerialPort(const std::string &port_name, std::uint32_t baud_rate);
    ~SerialPort();

    SerialPort(const SerialPort &) = delete;
    SerialPort &operator=(const SerialPort &) = delete;

    void write_all(const std::string &data);
    bool read_line(std::string &line, unsigned int timeout_ms);

private:
    void *handle_;
    std::string pending_data_;
};

} // namespace platform
