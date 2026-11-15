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

    bool is_stop_requested() const;

private:
    using SignalHandler = void (*)(int);
    SignalHandler previous_handler_;
};

class ImuStreamStopGuard
{
public:
    explicit ImuStreamStopGuard(SerialPort &serial_port);
    ~ImuStreamStopGuard();

    void stop();

private:
    SerialPort &serial_port_;
    bool active_;
};

} // namespace platform
