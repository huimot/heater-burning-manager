#include "Logger.h"
#include "ConfigAndHardwareManager.h"
#include "HeaterStateMachine.h"
#include "TcpControlServer.h"
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    std::string configPath = "config.json";
    if (argc > 1) {
        configPath = argv[1];
    }

    LOG("Starting Heater Burning Manager...");

    // Загружаем конфиг пинов и таймаутов
    if (!HardwareManager::instance().loadConfig(configPath)) {
        LOG("Warning: Could not load config file, using defaults.");
    }

    auto stateMachine = std::make_shared<HeaterStateMachine>();

    // Запускаем TCP сервер для связи с GUI на порту из конфига (или 5555 по умолчанию)
    int port = HardwareManager::instance().getListenPort();
    TcpControlServer server(port, stateMachine);
    server.start();

    LOG("Backend TCP Server started on port " + std::to_string(port));

    // Изолированный цикл автомата горения
    while (true) {
        stateMachine->process();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    return 0;
}
