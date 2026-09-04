//
// Created by admin on 31.08.2026.
//

#ifndef AT_SERVER_01_PTYPROCESSOR_H
#define AT_SERVER_01_PTYPROCESSOR_H

#include <cstdint>

#include "BootstrapApp.h" // for InitConfig

void process_main(Config& config);

void process_byte(uint8_t byte, Config& config);

void process_command(Config& config, const std::string& cur_command, bool forced_response_text);

bool match_pattern(const char* pattern, const char* str);

#endif //AT_SERVER_01_PTYPROCESSOR_H
