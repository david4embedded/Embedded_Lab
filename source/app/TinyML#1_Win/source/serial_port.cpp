/**
 * @file serial_port.cpp
 * @brief Implements Windows serial-port configuration and line I/O.
 * @date 2026-11-12
 * @author Sungsu Kim
 */

#include "serial_port.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#error "The serial port module currently supports Windows only."
#endif

namespace platform
{
constexpr std::size_t MAX_LINE_LENGTH = 1024;
constexpr std::size_t READ_BUFFER_SIZE = 256;
constexpr char DEVICE_PREFIX[] = "\\\\.\\";

static std::string NormalizePortName(const std::string &port_name);
static std::runtime_error MakeWindowsError(
    const std::string &operation,
    const DWORD error_code);
static HANDLE GetNativeHandle(void *handle);

class ScopedHandle
{
public:
    /**
     * @brief Owns a Windows handle until it is released or destroyed.
     * @retval None
     */
    explicit ScopedHandle(const HANDLE handle)
        : m_handle(handle)
    {
    }

    /**
     * @brief Closes the owned Windows handle when it remains valid.
     * @retval None
     */
    ~ScopedHandle()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
        }
    }

    /**
     * @brief Returns the currently owned Windows handle.
     * @retval handle The owned Windows handle
     */
    HANDLE get() const
    {
        return m_handle;
    }

    /**
     * @brief Transfers ownership of the Windows handle to the caller.
     * @retval released_handle The previously owned Windows handle
     */
    HANDLE release()
    {
        HANDLE result = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return result;
    }

private:
    HANDLE m_handle{};
};

/**
 * @brief Opens and configures a serial port.
 * @retval None
 */
SerialPort::SerialPort(const std::string &port_name, const std::uint32_t baud_rate)
    : m_handle(nullptr)
{
    const std::string device_name = NormalizePortName(port_name);
    ScopedHandle handle(CreateFileA(
        device_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE)
    {
        throw MakeWindowsError("Opening " + device_name, GetLastError());
    }

    if (!SetupComm(handle.get(), 4096, 4096))
    {
        throw MakeWindowsError("Configuring serial buffers", GetLastError());
    }

    DCB settings = DCB();
    settings.DCBlength = sizeof(settings);
    if (!GetCommState(handle.get(), &settings))
    {
        throw MakeWindowsError("Reading serial settings", GetLastError());
    }

    settings.BaudRate = baud_rate;
    settings.ByteSize = 8;
    settings.Parity = NOPARITY;
    settings.StopBits = ONESTOPBIT;
    settings.fBinary = TRUE;
    settings.fParity = FALSE;
    settings.fOutxCtsFlow = FALSE;
    settings.fOutxDsrFlow = FALSE;
    settings.fDtrControl = DTR_CONTROL_DISABLE;
    settings.fDsrSensitivity = FALSE;
    settings.fOutX = FALSE;
    settings.fInX = FALSE;
    settings.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(handle.get(), &settings))
    {
        throw MakeWindowsError("Setting serial parameters", GetLastError());
    }

    COMMTIMEOUTS timeouts = COMMTIMEOUTS();
    timeouts.ReadIntervalTimeout = MAXDWORD;
    if (!SetCommTimeouts(handle.get(), &timeouts))
    {
        throw MakeWindowsError("Setting serial timeouts", GetLastError());
    }

    if (!PurgeComm(handle.get(), PURGE_RXCLEAR | PURGE_TXCLEAR))
    {
        throw MakeWindowsError("Clearing serial buffers", GetLastError());
    }
    m_handle = handle.release();
}

/**
 * @brief Closes the serial port when it is open.
 * @retval None
 */
SerialPort::~SerialPort()
{
    if (m_handle != nullptr)
    {
        CloseHandle(GetNativeHandle(m_handle));
    }
}

/**
 * @brief Writes all bytes in the provided string to the serial port.
 * @retval None
 */
void SerialPort::writeAll(const std::string &data)
{
    HANDLE handle = GetNativeHandle(m_handle);
    std::size_t offset = 0;
    while (offset < data.size())
    {
        const DWORD requested = static_cast<DWORD>(
            std::min<std::size_t>(data.size() - offset, MAXDWORD));
        DWORD written = 0;
        if (!WriteFile(handle, data.data() + offset, requested, &written, nullptr))
        {
            throw MakeWindowsError("Writing to serial port", GetLastError());
        }
        if (written == 0)
        {
            throw std::runtime_error("Writing to serial port returned no data");
        }
        offset += written;
    }
}

/**
 * @brief Reads one newline-terminated serial input line before the timeout.
 * @retval true when a line is read
 * @retval false when the timeout expires before a line is read
 */
bool SerialPort::readLine(std::string &line, const std::uint32_t timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeout_ms);

    for (;;)
    {
        const std::size_t newline = m_pending_data.find('\n');
        if (newline != std::string::npos)
        {
            if (newline > MAX_LINE_LENGTH)
            {
                m_pending_data.erase(0, newline + 1);
                throw std::runtime_error("Serial input line exceeded 1024 bytes");
            }
            line.assign(m_pending_data, 0, newline);
            m_pending_data.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return true;
        }

        if (m_pending_data.size() > MAX_LINE_LENGTH)
        {
            m_pending_data.clear();
            throw std::runtime_error("Serial input line exceeded 1024 bytes");
        }

        char buffer[READ_BUFFER_SIZE];
        DWORD bytes_read = 0;
        if (!ReadFile(
                GetNativeHandle(m_handle),
                buffer,
                static_cast<DWORD>(sizeof(buffer)),
                &bytes_read,
                nullptr))
        {
            throw MakeWindowsError("Reading from serial port", GetLastError());
        }
        if (bytes_read > 0)
        {
            m_pending_data.append(buffer, bytes_read);
            continue;
        }

        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/**
 * @brief Normalizes a COM port name to its Windows device path.
 * @retval normalized_path The normalized Windows serial device path
 */
static std::string NormalizePortName(const std::string &port_name)
{
    if (port_name.compare(0, sizeof(DEVICE_PREFIX) - 1, DEVICE_PREFIX) == 0)
    {
        return port_name;
    }

    std::string upper_name = port_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
        [](const unsigned char character) -> char
        {
            return static_cast<char>(std::toupper(character));
        });

    if (upper_name.size() <= 3 || upper_name.compare(0, 3, "COM") != 0 ||
        !std::all_of(upper_name.begin() + 3, upper_name.end(),
            [](const unsigned char character) -> bool
            {
                return std::isdigit(character) != 0;
            }))
    {
        throw std::invalid_argument("Serial port must be named COM followed by a number");
    }

    return std::string(DEVICE_PREFIX) + upper_name;
}

/**
 * @brief Creates a runtime error containing the Windows error code.
 * @retval formatted_error The formatted Windows operation error
 */
static std::runtime_error MakeWindowsError(
    const std::string &operation,
    const DWORD error_code)
{
    return std::runtime_error(
        operation + " failed with Windows error " + std::to_string(error_code));
}

/**
 * @brief Converts the stored opaque handle to a Windows handle.
 * @retval native_handle The native Windows handle
 */
static HANDLE GetNativeHandle(void *const handle)
{
    return static_cast<HANDLE>(handle);
}

} // namespace platform
