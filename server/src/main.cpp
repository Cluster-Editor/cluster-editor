#include <cstdlib>
#include <iostream>
#include <string>

#include "app_version.h"

namespace {

void print_help(const std::string& executable) {
    std::cout << "Usage: " << executable << " [--help] [--version] [--port PORT]\n"
              << "\n"
              << "M0 foundation server placeholder. WebSocket support arrives in M3.\n";
}

bool is_port(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    const auto port = std::strtol(value.data(), nullptr, 10);
    return port > 0 && port <= 65535;
}

}  // namespace

int main(int argc, char* argv[]) {
    int port = 9001;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--help" || argument == "-h") {
            print_help(argv[0]);
            return 0;
        }

        if (argument == "--version") {
            std::cout << COLLAB_EDITOR_VERSION << '\n';
            return 0;
        }

        if (argument == "--port") {
            if (index + 1 >= argc || !is_port(argv[index + 1])) {
                std::cerr << "error: --port requires a value between 1 and 65535\n";
                return 2;
            }
            port = std::stoi(argv[++index]);
            continue;
        }

        std::cerr << "error: unknown argument: " << argument << '\n';
        return 2;
    }

    std::cout << "CollabEditor foundation server " << COLLAB_EDITOR_VERSION
              << " configured for port " << port << ".\n"
              << "WebSocket and CRDT services will be implemented in later milestones.\n";
    return 0;
}
