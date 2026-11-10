#pragma once
#include "IPin.h"
#include "ModbusPin.h"
#include "DummyPin.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <unordered_map>

class HardwareManager {
public:
    static HardwareManager& instance() {
        static HardwareManager inst;
        return inst;
    }

    bool loadConfig(const std::string& configPath) {
        std::ifstream file(configPath);
        if (!file.is_open()) return false;
        
        nlohmann::json j;
        file >> j;

        // 1. Читаем общие настройки
        m_logPath = j.value("log_path", "./logs/");
        m_listenPort = j["network"].value("listen_port", 5555);
        
        for (auto& [key, value] : j["timeouts_sec"].items()) {
            m_timeouts[key] = value.get<int>();
        }

        // 2. Инициализируем Modbus-мастера (соединения с блоками реле)
        for (auto& master : j["modbus_masters"]) {
            std::string id = master["id"];
            std::string ip = master["ip"];
            int port = master["port"];
            
            modbus_t* ctx = modbus_new_tcp(ip.c_str(), port);
            if (ctx && modbus_connect(ctx) == 0) {
                m_modbusContexts[id] = ctx;
            } else {
                // Логируем ошибку инициализации конкретного блока
            }
        }

        // 3. Создаем пины на основе их драйверов
        for (auto& p : j["pins"]) {
            std::string name = p["name"];
            PinDirection dir = (p["dir"] == "input") ? PinDirection::INPUT_PIN : PinDirection::OUTPUT_PIN;
            std::string driver = p["driver"];

            std::shared_ptr<IPin> pinObj;

            if (driver == "modbus") {
                std::string masterId = p["master_id"];
                int address = p["address"];
                bool inverted = p.value("inverted", false);
                
                if (m_modbusContexts.count(masterId)) {
                    pinObj = std::make_shared<ModbusPin>(name, dir, m_modbusContexts[masterId], address, inverted);
                }
            } 
            else if (driver == "dummy") {
                pinObj = std::make_shared<DummyPin>(name, dir);
            }
            // Сюда в будущем добавляется: else if (driver == "hudbas") { ... }

            if (pinObj) {
                m_pins[name] = pinObj;
            }
        }
        return true;
    }

    std::shared_ptr<IPin> getPin(const std::string& name) {
        if (m_pins.count(name)) return m_pins[name];
        return std::make_shared<DummyPin>(name, PinDirection::INPUT_PIN); // Защита от nullptr
    }

    int getTimeout(const std::string& key, int defaultVal) {
        if (m_timeouts.count(key)) return m_timeouts[key];
        return defaultVal;
    }
    
    int getListenPort() const { return m_listenPort; }
    std::string getLogPath() const { return m_logPath; }

private:
    HardwareManager() = default;
    std::string m_logPath;
    int m_listenPort = 5555;
    std::map<std::string, int> m_timeouts;
    std::unordered_map<std::string, modbus_t*> m_modbusContexts;
    std::unordered_map<std::string, std::shared_ptr<IPin>> m_pins;
};
