//
// Created by admin on 31.08.2026.
//

#include <iostream> // std::cout
#include <algorithm> // std::find
#include <unistd.h> // for close(), read()

#include "BootstrapApp.h"
#include "PtyProcessor.h"


void process_main(Config& config) {
    // Используем буферное чтение, для оптимизации вызовов read()
    uint8_t rx_buffer[255];

    // Main loop
    while (true) {
        ssize_t readLen = read(config.pty_fd, rx_buffer, sizeof(rx_buffer));

        // @todo - попробовать перейти на event-based модель опроса через poll()
        if (readLen > 0) {
            for(ssize_t i = 0; i < readLen; ++i) {
                uint8_t rx_byte = rx_buffer[i];
                process_byte(rx_byte, config);
            }
        }

        if (readLen < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нечего читать, модем занялся своими делами. Пауза, чтобы процесс не занял все 100% ЦП
                usleep(1000); // 1ms
            } else {
                std::cout << "readLen: " << readLen << '\n';
                std::perror("\n[ERROR] Wrong reading from device.");
                break;
            }
        }
        if (readLen == 0) {
            std::cout << "\n[TTY] End of file (EOF) detected on device.\n";
            break;
        }
    }
}

bool has_vocab_key(const Config& config, const char* str) {
    auto it = std::find(config.vocab_order.begin(), config.vocab_order.end(), str);
    return it != config.vocab_order.end();
}

void process_byte(uint8_t byte, Config& config)
{
    // Статический буфер. Храним состояние между вызовами функции
    static std::string current_command;
    // Статический буфер. Последняя команда
    static std::string last_command;
    // Флаг принудительной установки ответа (при обработке определенных команд, возвращаем ОК, не проверяя словарь)
    bool forced_response_text = false;
    static bool in_quotes = false;

    // Нажали Backspace (0x08) или DEL (0x7F)
    if (byte == 0x08 || byte == 0x7F) {
        if (!current_command.empty()) {

            // последний символ должен быть удален из памяти, затем выведено: сдвиг назад, пробел, сдвиг назад
            current_command.pop_back();
            if (config.echo_input) {
                uint8_t backspace_sequence[] = {0x08, 0x20, 0x08};
                write(config.pty_fd, backspace_sequence, 3);
            }
        }
        return;
    }

    // Нажали Enter (0x0D)
    if (byte == 0x0D) {

        // @debug
        //std::cout << "[PTY] Получена команда: \"" << current_command << "\"\n";

        // Особенность терминала - при нажатии enter переводит курсор на одну лишнюю строку вниз
        // На эмуляцию работы модема это не влияет, он возвращает по стандарту: \r\nTEXT\r\n
        if (config.echo_input) {
            uint8_t crlf[] = {0x0D, 0x0A};
            write(config.pty_fd, crlf, 2);
        }

        // Если ничего не было введено, то ничего и не делаем
        if (current_command.empty()) {
            return;
        }

        // Единственная команда, которую обрабатываем (ее может не быть в словаре!)
        // @todo - вынести в отдельный обработчик, если будут добавляться другие команды (S-registers, ATV итд)
        // если команды есть в словаре, то берем ответ из нее, иначе - хардкод
        if (current_command == "ATE0" || current_command == "ATE") {
            config.echo_input = false;
            if (!(has_vocab_key(config, "ATE") && has_vocab_key(config, "ATE0"))) {
                forced_response_text = true;
            }
        }
        if (current_command == "ATE1") {
            config.echo_input = true;
            // если команда есть в словаре, то берем ответ из нее, иначе - хардкод
            if (!has_vocab_key(config, "ATE1")) {
                forced_response_text = true;
            }
        }

        last_command = current_command;

        process_command(config, current_command, forced_response_text);

        current_command.clear();

        return;
    }

    // Копим символы в буфер, сразу делаем uppercase для совпадения с командой
    if (byte == '"') in_quotes = !in_quotes;
    if (!in_quotes) {
        current_command += static_cast<char>(std::toupper(static_cast<char>(byte)));
    } else {
        current_command += static_cast<char>(static_cast<char>(byte));
    }

    // @debug
    //std::cout << make_printable(current_command) << '\n';

    // Сразу вызываем последнюю команду, не дожидаясь терминатора
    // Здесь будет некрасиво в консоли
    if (current_command == "A/") {
        if (last_command.empty()) {
            process_command(config, last_command, true);
        } else {
            process_command(config, last_command, false);
        }

        current_command.clear();
    }

    // Выводим символ (если не отменялось ранее командой ATE0)
    if (config.echo_input) {
        write(config.pty_fd, &byte, 1);
    }
}

void process_command(Config& config, const std::string& cur_command, bool forced_response_text)
{
    std::string response;

    // Не очень хорошее решение, но для одной команды подойдет
    if (forced_response_text) {
        response = "\r\nOK\r\n";
    } else {
        for(const auto& key: config.vocab_order) {
            const auto value = config.vocab.at(key);

            // @debug
            //std::cout << make_printable(key) << "---" << ::make_printable(value) << '\n';

            if (match_pattern(key.c_str(), cur_command.c_str())) {

                response = value;
                break;
            } else {
                response = "\r\nERROR\r\n";
            }
        }
    }

    write(config.pty_fd, response.c_str(), response.length());
}

/**
 * Классическая рекурсивная функция сопоставления по . и *
 */
bool match_pattern(const char* pattern, const char* str) {

    // Если оба указателя дошли до конца — это полный матч
    if (*pattern == '\0' && *str == '\0') return true;

    // Обработка '*' - ноль или более любых символов
    if (*pattern == '*') {
        // Проверяем гипотезу: '*' поглотила 0 символов, смотрим следующий символ маски
        if (match_pattern(pattern + 1, str)) {
            return true;
        }
        // Если не подошло, сдвигаем строку (поглощаем 1 символ) и пробуем снова
        if (*str != '\0') {
            return match_pattern(pattern, str + 1);
        }
        return false;
    }

    // Обработка '.' - один любой символ, либо точное совпадение знаков
    if ((*pattern == '.' && *str != '\0') || *pattern == *str) {
        return match_pattern(pattern + 1, str + 1);
    }

    // Не совпала ни *, ни ., ни символы, результат: строки не совпали
    return false;
}
