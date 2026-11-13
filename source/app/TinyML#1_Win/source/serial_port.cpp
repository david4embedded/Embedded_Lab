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

namespace platform {
namespace {

constexpr std::size_t kMaxLineLength = 1024;
constexpr std::size_t kReadBufferSize = 256;

std::string normalize_port_name(const std::string &port_name)
{
    const std::string device_prefix = "\\\\.\\";
    if (port_name.compare(0, device_prefix.size(), device_prefix) == 0) {
        return port_name;
    }

    std::string upper_name = port_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    if (upper_name.size() <= 3 || upper_name.compare(0, 3, "COM") != 0 ||
        !std::all_of(upper_name.begin() + 3, upper_name.end(),
            [](unsigned char character) {
                return std::isdigit(character) != 0;
            })) {
        throw std::invalid_argument("Serial port must be named COM followed by a number");
    }

    return device_prefix + upper_name;
}

std::runtime_error make_windows_error(const std::string &operation, DWORD error_code)
{
    return std::runtime_error(
        operation + " failed with Windows error " + std::to_string(error_code));
}

HANDLE native_handle(void *handle)
{
    return static_cast<HANDLE>(handle);
}

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}

    ~ScopedHandle()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const
    {
        return handle_;
    }

    HANDLE release()
    {
        HANDLE handle = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return handle;
    }

private:
    HANDLE handle_;
};

} // namespace

SerialPort::SerialPort(const std::string &port_name, std::uint32_t baud_rate)
    : handle_(nullptr)
{
    const std::string device_name = normalize_port_name(port_name);
    ScopedHandle handle(CreateFileA(
        device_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        throw make_windows_error("Opening " + device_name, GetLastError());
    }

    if (!SetupComm(handle.get(), 4096, 4096)) {
        throw make_windows_error("Configuring serial buffers", GetLastError());
    }

    DCB settings{};
    settings.DCBlength = sizeof(settings);
    if (!GetCommState(handle.get(), &settings)) {
        throw make_windows_error("Reading serial settings", GetLastError());
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
    if (!SetCommState(handle.get(), &settings)) {
        throw make_windows_error("Setting serial parameters", GetLastError());
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    if (!SetCommTimeouts(handle.get(), &timeouts)) {
        throw make_windows_error("Setting serial timeouts", GetLastError());
    }

    if (!PurgeComm(handle.get(), PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        throw make_windows_error("Clearing serial buffers", GetLastError());
    }
    handle_ = handle.release();
}

SerialPort::~SerialPort()
{
    if (handle_ != nullptr) {
        CloseHandle(native_handle(handle_));
    }
}

void SerialPort::write_all(const std::string &data)
{
    HANDLE handle = native_handle(handle_);
    std::size_t offset = 0;
    while (offset < data.size()) {
        const DWORD requested = static_cast<DWORD>(
            std::min<std::size_t>(data.size() - offset, MAXDWORD));
        DWORD written = 0;
        if (!WriteFile(handle, data.data() + offset, requested, &written, nullptr)) {
            throw make_windows_error("Writing to serial port", GetLastError());
        }
        if (written == 0) {
            throw std::runtime_error("Writing to serial port returned no data");
        }
        offset += written;
    }
}

bool SerialPort::read_line(std::string &line, unsigned int timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeout_ms);

    for (;;) {
        const std::size_t newline = pending_data_.find('\n');
        if (newline != std::string::npos) {
            if (newline > kMaxLineLength) {
                pending_data_.erase(0, newline + 1);
                throw std::runtime_error("Serial input line exceeded 1024 bytes");
            }
            line.assign(pending_data_, 0, newline);
            pending_data_.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }

        if (pending_data_.size() > kMaxLineLength) {
            pending_data_.clear();
            throw std::runtime_error("Serial input line exceeded 1024 bytes");
        }

        char buffer[kReadBufferSize];
        DWORD bytes_read = 0;
        if (!ReadFile(
                native_handle(handle_),
                buffer,
                static_cast<DWORD>(sizeof(buffer)),
                &bytes_read,
                nullptr)) {
            throw make_windows_error("Reading from serial port", GetLastError());
        }
        if (bytes_read > 0) {
            pending_data_.append(buffer, bytes_read);
            continue;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace platform
