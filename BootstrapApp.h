//
// Created by admin on 31.08.2026.
//

#ifndef AT_SERVER_01_BOOTSTRAPAPP_H
#define AT_SERVER_01_BOOTSTRAPAPP_H

#include <string>
#include <vector>
#include <unordered_map>

// @debug - отладочная функция
std::string make_printable(const std::string& src);

struct Config {
  const std::string pty_path;
  const int pty_fd;
  const std::string vocab_file; // set "vocab_default.txt" if empty
  const int vocab_fd; // vocab_file FD

  // Флаг "эхо при вводе символов", управляется командами: ATE0/ATE1
  bool echo_input = true;

  // Vocabulary <expect=answer>
  const std::unordered_map<std::string, std::string> vocab;
  // хранит для словаря ключи в том порядке, в каком они идут в файле
  const std::vector<std::string> vocab_order;

  std::string last_command_key;

  Config(std::string pty_path,
          int pty_fd,
          std::string vocab_file,
          int vocab_fd,
          std::unordered_map<std::string,std::string> vocab,
          std::vector<std::string> vocab_order
        ) :
          pty_path(std::move(pty_path)), pty_fd(pty_fd), vocab_file(std::move(vocab_file)), vocab_fd(vocab_fd),
          vocab(std::move(vocab)), vocab_order(std::move(vocab_order)) { }
};

Config bootstrap_app(int argc, char* argv[]);

#endif //AT_SERVER_01_BOOTSTRAPAPP_H
