/**
 * @file serial_port.hpp
 * @brief Declares the Windows serial-port interface used by live IMU input.
 * @date 2026-11-12
 * @author Sungsu Kim
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace platform
{

class SerialPort
{
public:
    SerialPort(const std::string &port_name, const std::uint32_t baud_rate);
    ~SerialPort();

    SerialPort(const SerialPort &) = delete;
    SerialPort &operator=(const SerialPort &) = delete;

    void writeAll(const std::string &data);
    bool readLine(std::string &line, const std::uint32_t timeout_ms);

private:
    void *m_handle{};
    std::string m_pending_data{};
};

} // namespace platform
