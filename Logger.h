#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string timeStr = currentDateTime();
        
        // Вывод в консоль Qt Creator / Терминала
        std::cout << "[" << timeStr << "] " << message << std::endl;
        
        // Вывод в файл
        if (m_file.is_open()) {
            m_file << "[" << timeStr << "] " << message << std::endl;
        }
    }

private:
    Logger() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        // Файл лога будет создаваться автоматически с текущей датой
        ss << "log_" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d") << ".txt";
        m_file.open(ss.str(), std::ios_base::out | std::ios_base::app);
    }
    
    ~Logger() { 
        if (m_file.is_open()) m_file.close(); 
    }

    std::string currentDateTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
        return ss.str();
    }

    std::ofstream m_file;
    std::mutex m_mutex;
};

// Макрос для быстрой отправки логов из любого места кода
#define LOG(msg) Logger::instance().log(msg)
