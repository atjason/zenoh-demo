#include "zenoh.hxx"

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

        std::cout << "Opened zenoh session. Declared subscriber on key: " << key << std::endl;

        auto subscriber = session.declare_subscriber(
            KeyExpr(key),
            [](const Sample& sample) {
                std::cout << "[C++ sub] Received ('" << sample.get_keyexpr().as_string()
                          << "': '" << sample.get_payload().as_string() << "')" << std::endl;
            },
            closures::none);

        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        (void)subscriber;
        std::cout << "Shutting down C++ subscriber..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in C++ subscriber: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

