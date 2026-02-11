#include "zenoh.hxx"

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using namespace zenoh;

static bool running = true;

void handle_signal(int) {
    running = false;
}

int main(int argc, char** argv) {
    const std::string key = "demo/zenoh/getting-started";

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        Config config = Config::create_default();
        config.insert_json5("connect/endpoints", R"(["tcp/127.0.0.1:7447"])");
        auto session = Session::open(std::move(config));

        std::cout << "Opened zenoh session. Declaring publisher on key: " << key << std::endl;
        auto publisher = session.declare_publisher(KeyExpr(key));

        std::size_t count = 0;
        while (running) {
            std::string msg = "Hello from C++ #" + std::to_string(count++);
            std::cout << "[C++ pub] Putting Data ('" << key << "': '" << msg << "')" << std::endl;
            publisher.put(msg);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Shutting down C++ publisher..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in C++ publisher: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

