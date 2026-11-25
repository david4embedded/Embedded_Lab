/**
 * @file live_guards.hpp
 * @brief RAII guards for interrupt handling and live IMU stream shutdown.
 * @date 2026-11-14
 * @author Sungsu Kim
 */

#pragma once

#include "serial_port.hpp"

namespace platform
{
class InterruptHandlerGuard
{
public:
    InterruptHandlerGuard();
    ~InterruptHandlerGuard();

    InterruptHandlerGuard(const InterruptHandlerGuard &) = delete;
    InterruptHandlerGuard &operator=(const InterruptHandlerGuard &) = delete;

    bool isStopRequested() const;

private:
    using SignalHandler = void (*)(int);
    SignalHandler m_previous_handler{};
};

class ImuStreamStopGuard
{
public:
    explicit ImuStreamStopGuard(SerialPort &serial_port);
    ~ImuStreamStopGuard();

    void stop();

private:
    SerialPort &m_serial_port;
    bool m_active{};
};

} // namespace platform
