/**
 * @file live_guards.cpp
 * @brief Implements interrupt handling and live IMU stream shutdown guards.
 * @date 2026-11-14
 * @author Sungsu Kim
 */

#include "live_guards.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>

namespace platform
{
static volatile std::sig_atomic_t s_stop_requested = 0;
static void HandleInterrupt(const int);

/**
 * @brief Registers the interrupt handler and clears any prior stop request.
 * @retval None
 */
InterruptHandlerGuard::InterruptHandlerGuard()
    : m_previous_handler(nullptr)
{
    s_stop_requested = 0;
    m_previous_handler = std::signal(SIGINT, HandleInterrupt);
    if (m_previous_handler == SIG_ERR)
    {
        throw std::runtime_error("Could not register Ctrl+C handler");
    }
}

/**
 * @brief Restores the previously registered interrupt handler.
 * @retval None
 */
InterruptHandlerGuard::~InterruptHandlerGuard()
{
    std::signal(SIGINT, m_previous_handler);
}

/**
 * @brief Reports whether an interrupt signal has requested shutdown.
 * @retval true when shutdown was requested
 * @retval false otherwise
 */
bool InterruptHandlerGuard::isStopRequested() const
{
    return s_stop_requested != 0;
}

/**
 * @brief Creates a guard that stops the board IMU stream when destroyed.
 * @retval None
 */
ImuStreamStopGuard::ImuStreamStopGuard(SerialPort &serial_port)
    : m_serial_port(serial_port),
      m_active(true)
{
}

/**
 * @brief Stops the board IMU stream if it is still active.
 * @retval None
 */
ImuStreamStopGuard::~ImuStreamStopGuard()
{
    if (m_active)
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

/**
 * @brief Sends the stop key to the board once.
 * @retval None
 */
void ImuStreamStopGuard::stop()
{
    if (m_active)
    {
        m_serial_port.writeAll(std::string(1, '\x1b'));
        m_active = false;
    }
}

/**
 * @brief Records that the process received an interrupt signal.
 * @retval None
 */
static void HandleInterrupt(const int)
{
    s_stop_requested = 1;
}

} // namespace platform
