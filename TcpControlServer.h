#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>
#include "HeaterStateMachine.h"

class TcpControlServer {
public:
    TcpControlServer(int port, std::shared_ptr<HeaterStateMachine> sm) 
        : m_port(port), m_sm(sm), m_running(false), m_serverFd(-1) {}

    ~TcpControlServer() { stop(); }

    void start() {
        m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        bind(m_serverFd, (struct sockaddr*)&address, sizeof(address));
        listen(m_serverFd, 3);

        m_running = true;
        m_serverThread = std::thread(&TcpControlServer::serverLoop, this);
    }

    void stop() {
        m_running = false;
        if (m_serverFd != -1) {
            close(m_serverFd);
        }
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

private:
    void serverLoop() {
        while (m_running) {
            sockaddr_in clientAddr{};
            socklen_t addrLen = sizeof(clientAddr);
            // В реальном проекте здесь лучше использовать select() или epoll() с таймаутом,
            // чтобы поток мог корректно завершиться при m_running == false.
            int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &addrLen);
            
            if (clientFd >= 0) {
                std::thread(&TcpControlServer::handleClient, this, clientFd).detach();
            }
        }
    }

    void handleClient(int clientFd) {
        char buffer[1024] = {0};
        while (m_running) {
            int valRead = read(clientFd, buffer, 1024);
            if (valRead <= 0) break; // Клиент отключился (например GUI закрыли)

            std::string request(buffer, valRead);
            
            // Обрабатываем команду от Qt GUI (Парсим команду "heat", "stop" и т.д.)
            // В идеале GUI шлет JSON, например: {"command": "heat"}
            if (request.find("heat") != std::string::npos) m_sm->setCommand("heat");
            if (request.find("stop") != std::string::npos) m_sm->setCommand("stop");

            // Отправляем обратно в GUI текущий статус автомата
            std::string response = "{\"status\":" + std::to_string((int)m_sm->getStatus()) + "}\n";
            send(clientFd, response.c_str(), response.length(), 0);
        }
        close(clientFd);
    }

    int m_port;
    std::shared_ptr<HeaterStateMachine> m_sm;
    std::atomic<bool> m_running;
    int m_serverFd;
    std::thread m_serverThread;
};
