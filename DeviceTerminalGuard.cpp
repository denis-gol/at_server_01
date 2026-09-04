//
// Created by admin on 31.08.2026.
//

#include <iostream>

#include <unistd.h> // for isatty()
#include "DeviceTerminalGuard.h"

DeviceTerminalGuard::DeviceTerminalGuard(int fd) : m_fd(fd) {
    if (m_fd < 0) return;

    if (!isatty(m_fd)) {
        throw std::runtime_error("The input device is not a TTY.");
    }

    // Сохраняем старую конфигурацию
    if (tcgetattr(m_fd, &m_orig_termios) != 0) {
        throw std::runtime_error("tcgetattr (termios) failed.");
    }
    m_is_valid = true;

    // Raw mode (== stty -icanon -icrnl -onlcr -echo)
    struct termios raw = m_orig_termios;

    // i-flags: turn off mapping CR/LF, software flow control
    raw.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR | IGNCR);
    // o-flags: полностью отключаем пост-обработку вывода
    raw.c_oflag &= ~(ONLCR | OCRNL);
    // l-flags: побайтовый ввод (non-canonical), отключаем локальное эхо и сигналы
    raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    // Настройки блокировки: read() блокируется, пока не появится хотя бы 1 байт
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(m_fd, TCSAFLUSH, &raw) != 0) {
        throw std::runtime_error("Не удалось установить raw mode для устройства.");
    }
}

DeviceTerminalGuard::~DeviceTerminalGuard() {
    if (m_is_valid) {
        // Возвращаем TTY в исходное состояние при выходе из области видимости
        tcsetattr(m_fd, TCSAFLUSH, &m_orig_termios);
    }
}

