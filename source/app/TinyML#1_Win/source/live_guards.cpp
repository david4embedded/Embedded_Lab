#include "live_guards.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>

namespace platform
{
static volatile std::sig_atomic_t stop_requested = 0;
static void handle_interrupt(int)
{
    stop_requested = 1;
}

InterruptHandlerGuard::InterruptHandlerGuard()
    : previous_handler_(nullptr)
{
    stop_requested = 0;
    previous_handler_ = std::signal(SIGINT, handle_interrupt);
    if (previous_handler_ == SIG_ERR)
    {
        throw std::runtime_error("Could not register Ctrl+C handler");
    }
}

InterruptHandlerGuard::~InterruptHandlerGuard()
{
    std::signal(SIGINT, previous_handler_);
}

bool InterruptHandlerGuard::is_stop_requested() const
{
    return stop_requested != 0;
}

ImuStreamStopGuard::ImuStreamStopGuard(SerialPort &serial_port)
    : serial_port_(serial_port),
      active_(true)
{
}

ImuStreamStopGuard::~ImuStreamStopGuard()
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

void ImuStreamStopGuard::stop()
{
    if (active_)
    {
        serial_port_.write_all(std::string(1, '\x1b'));
        active_ = false;
    }
}

} // namespace platform
