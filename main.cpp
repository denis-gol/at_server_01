#include <iostream>
#include <unistd.h>    // for close()

#include "BootstrapApp.h"
#include "DeviceTerminalGuard.h"
#include "PtyProcessor.h"

int main(int argc, char* argv[])
{
    Config config = bootstrap_app(argc, argv);

    std::cout << "[CLI] Path to TTY: " << config.pty_path << "\n";
    std::cout << "[CLI] Vocab file: " << config.vocab_file << "\n";

    try {
        DeviceTerminalGuard ttyGuard(config.pty_fd);
        std::cout << "[TTY] Device in RAW MODE.\n";
        std::cout << "[TTY] Listening input bytes... (Press Ctrl+C for exit)\n";
        std::cout << "------------------------------------------------------\n";

        process_main(config);

    }
    catch (const std::exception& ex) {
        std::cerr << "\n[CRITICAL ERROR] " << ex.what() << "\n";
        close(config.pty_fd);
        return 1;
    }

    close(config.pty_fd);
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << "[FINISH] The device is closed, the terminal has been successfully restored.\n";

    return 0;
}
