//
// Created by admin on 31.08.2026.
//

#ifndef AT_SERVER_01_DEVICETERMINALGUARD_H
#define AT_SERVER_01_DEVICETERMINALGUARD_H

#include <termios.h>

class DeviceTerminalGuard {
public:
    explicit DeviceTerminalGuard(int fd);
    ~DeviceTerminalGuard();

    // RAII-secure
    DeviceTerminalGuard(const DeviceTerminalGuard&) = delete;
    DeviceTerminalGuard& operator=(const DeviceTerminalGuard&) = delete;

private:
    int m_fd;
    bool m_is_valid = false;
    struct termios m_orig_termios{};
};

#endif //AT_SERVER_01_DEVICETERMINALGUARD_H
