//
// Created by admin on 31.08.2026.
//
#include <iostream>

//#include <fcntl.h>     // for open()
#include <sys/file.h>  // For flock()
#include <unistd.h> // for close(), read()
#include <stdexcept>
#include <cstring>

#include "BootstrapApp.h"

// @debug - отладочная функция
std::string make_printable(const std::string& src) {
    std::string printable;
    for (char ch : src) {
        if (ch == '\r') printable += "\\r";
        else if (ch == '\n') printable += "\\n";
        else if (ch == '\t') printable += "\\t";
        else printable += ch;
    }
    return printable;
}

// DECLARATIONS
void parse_vocab(
        int fd,
        std::unordered_map<std::string,
        std::string>& vocab,
        std::vector<std::string>& vocab_order,
        int& error_lines_counter
);

std::pair<std::string, std::string> parse_line(std::string& line, bool& is_line_valid);

// IMPLEMENTATIONS
Config bootstrap_app(int argc, char* argv[]) {
    try {
        int error_lines_counter = 0;

        std::string pty_path;
        std::string vocab_file;
        // Очень простой парсер командной строки
        for (int i = 1; i<argc; ++i) {
            std::string arg = argv[i];

            if (arg=="--cfile") {
                if (i+1<argc) {
                    vocab_file = argv[++i];
                }
                else {
                    throw std::runtime_error("flag --cfile require value.");
                }
            }
            else {
                pty_path = arg;
            }
        }

        if (pty_path.empty()) {
            throw std::runtime_error(std::string("Using: ")+argv[0]+" <path_to_pty> [--cfile <file>])");
        }
        std::cout << "[Server] Initializing... Opening port: " << pty_path << '\n';


        // Пробуем открыть файлы

        // @todo - сделать блокировку консоли (через lock). Сейчас 2-ой сервер бесконечно ждет освобождения tty
        // Неблокирующий режим tty - эмулируем работу модема: опросили порт и побежали дальше
        int pty_fd = open(pty_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (pty_fd<0) {
            throw std::runtime_error(
                    std::string("Failed to open tty: ")+std::strerror(errno));
        }

        // Выставляем Advisory Lock блокировку консоли (на всякий случай, сейчас она по факту не работает)
        if (flock(pty_fd, LOCK_EX | LOCK_NB)==-1) {
            throw std::runtime_error(
                    std::string("Failed to lock file: ")+std::strerror(errno));
        }

        // @todo - добавить проверку типа файла
        int vocab_fd = open(vocab_file.c_str(), O_RDONLY);
        if (vocab_fd<0) {
            throw std::runtime_error(
                    std::string("Failed to open vocabulary file: ")+std::strerror(errno));
        }

        // Загружаем словарь
        std::unordered_map<std::string, std::string> vocab;
        std::vector<std::string> vocab_order;
        parse_vocab(vocab_fd, vocab, vocab_order, error_lines_counter);
        if (error_lines_counter>0) {
            std::cout << "[WARNING] " << error_lines_counter << " line(s) was skipped when parse vocab." << '\n';
        }

        Config config = Config(std::move(pty_path), pty_fd,
                std::move(vocab_file), vocab_fd,
                std::move(vocab), std::move(vocab_order)
        );

        return config;
    }
    catch (std::runtime_error& error) {
        std::cerr << "[CRITICAL] Failed to bootstrap server: " << error.what() << std::endl;
        std::exit(1);
    }
}

/**
 * Читаем файл, заполняем словарь, разбиваем построчно, удаляем переносы строк (\n | \r\n) в конце строки
 */
void parse_vocab(int fd, std::unordered_map<std::string, std::string>& vocab, std::vector<std::string>& vocab_order,
        int& error_lines_counter)
{
    std::string line;
    char ch;
    bool is_line_valid = false;

    // @todo - фича: ошибочные строки не отбрасывать, а складывать в отдельный файл (выводить лог)
    while(read(fd, &ch, 1) == 1) {
        if (ch == '\n') {
            // удаляем 0x0D в конце, если вдруг там CRLF
            if (!line.empty() && line.back() == '\r') line.pop_back();
            // это была пустая строка или комментарий
            if (line.empty() || line[0] == '#') {
                line.clear();
                continue;
            }

            auto parsed_pair = parse_line(line, is_line_valid);
            if (is_line_valid) {
                if (vocab.find(parsed_pair.first) == vocab.end()) {
                    vocab_order.push_back(parsed_pair.first);
                }
                vocab[parsed_pair.first] = parsed_pair.second;
            } else {
                ++error_lines_counter;
            }

            line.clear();
        } else {
            line += ch;
        }
    }
}


/**
 * Распарсить строку
 */
#include <string>
#include <utility>
#include <cctype>

// Сразу экранируем управляющие символы (\r, \n, \t итд) в строке ответа
std::string unescape_answer(const std::string& src) {
    std::string res;
    res.reserve(src.length());
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '\\' && i + 1 < src.length()) {
            switch (src[++i]) {
            case 'r':  res += '\r'; break;
            case 'n':  res += '\n'; break;
            case 't':  res += '\t'; break;
            case '"':  res += '"';  break;
            case '\\': res += '\\'; break;
            default:
                // Если последовательность неизвестна, сохраняем как текст
                res += '\\';
                res += src[i];
                break;
            }
        } else {
            res += src[i];
        }
    }

    // Делаем обрамление (Framing по стандарту V.250)
    std::string final_res;

    // Проверяем и добавляем префикс <CR><LF> (0x0D, 0x0A), если его нет в начале
    if (res.length() < 2 || res[0] != '\r' || res[1] != '\n') {
        final_res += "\r\n";
    }
    final_res += res;

    // Проверяем и добавляем суффикс <CR><LF> (0x0D, 0x0A), if его нет в конце
    if (final_res.length() >= 2) {
        size_t len = final_res.length();
        if (final_res[len - 2] != '\r' || final_res[len - 1] != '\n') {
            final_res += "\r\n";
        }
    }

    return final_res;
}

// удаление пробелов, табов и приведение к верхн. регистру вне кавычек
std::string normalize_request(const std::string& src) {
    std::string res;
    res.reserve(src.length());
    bool in_quotes = false;
    for (char ch : src) {
        if (ch == '"') {
            in_quotes = !in_quotes;
        }
        if ((ch == ' ' || ch == '\t') && !in_quotes) {
            continue; // Выбрасываем пробелы и табы вне кавычек
        }
        if (!in_quotes) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        res += ch;
    }
    return res;
}

std::pair<std::string, std::string> parse_line(std::string& line, bool& is_line_valid)
{
    std::string expect;
    std::string answer;
    is_line_valid = false; // negative result
    std::pair<std::string, std::string> response = std::make_pair("", "");

    // Строка слишком короткая даже для "AT="
    if (line.length()<3) return response;

    // Проверяем префикс
    if (!(line.substr(0, 2)=="AT" || line.substr(0, 2)=="at"))
        return response;

    // Ищем уникальный маркер-разделитель "\r=" (три символа: '\', 'r', '=')
    size_t delim_pos = line.find("\\r=");

    // если маркер не найден, либо строка им начинается/заканчивается
    if (delim_pos == std::string::npos || delim_pos == 0 || delim_pos + 3 >= line.length()) {
        return response;
    }

    // Режем строку по радзелителю
    std::string raw_expect = line.substr(0, delim_pos);
    std::string raw_answer = line.substr(delim_pos + 3);

    response.first = normalize_request(raw_expect);
    response.second  = unescape_answer(raw_answer);

    is_line_valid = true;

    return response;
}