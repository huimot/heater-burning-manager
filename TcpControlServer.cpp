#include "TcpControlServer.h"
#include "HeaterStateMachine.h"
#include "Logger.h"
#include <cstring>
#include <iostream>

TcpControlServer::TcpControlServer(int port, std::shared_ptr<HeaterStateMachine> sm)
    : m_port(port)
    , m_serverFd(-1)
    , m_running(false)
    , m_sm(sm) {}

TcpControlServer::~TcpControlServer() {
    stop();
}

void TcpControlServer::start() {
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) {
        LOG("TCP Server: Failed to create socket");
        return;
    }

    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    if (bind(m_serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG("TCP Server: Bind failed on port " + std::to_string(m_port));
        return;
    }

    if (listen(m_serverFd, 3) < 0) {
        LOG("TCP Server: Listen failed");
        return;
    }

    m_running = true;
    m_serverThread = std::thread(&TcpControlServer::serverLoop, this);
}

void TcpControlServer::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_serverFd != -1) {
        shutdown(m_serverFd, SHUT_RDWR);
        close(m_serverFd);
        m_serverFd = -1;
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    LOG("TCP Server stopped.");
}

void TcpControlServer::serverLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        int clientFd = accept(m_serverFd, (struct sockaddr*)&clientAddr, &addrLen);

        if (clientFd >= 0) {
            if (!m_running) {
                close(clientFd);
                break;
            }
            std::thread(&TcpControlServer::handleClient, this, clientFd).detach();
        } else {
            break;
        }
    }
}

void TcpControlServer::handleClient(int clientFd) {
    char buffer[1024]; // Полноценный буфер на 1 Кб
    struct timeval tv;
    tv.tv_sec = 30; // 30 секунд таймаут удержания сессии без активности
    tv.tv_usec = 0;
    setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    while (m_running) {
        std::memset(buffer, 0, sizeof(buffer));
        int valRead = read(clientFd, buffer, sizeof(buffer) - 1);
        if (valRead <= 0) break; // Отключение GUI или таймаут

        std::string request(buffer, valRead);

        if (request.find("heat") != std::string::npos)      m_sm->setCommand("heat");
        if (request.find("stop") != std::string::npos)      m_sm->setCommand("stop");
        if (request.find("auto") != std::string::npos)      m_sm->setCommand("auto");
        if (request.find("stopauto") != std::string::npos)  m_sm->setCommand("stopauto");
        if (request.find("clearerror") != std::string::npos)m_sm->setCommand("clearerror");

        std::string response = "{\"status\":" + std::to_string(static_cast<int>(m_sm->getStatus())) + "}\n";
        if (send(clientFd, response.c_str(), response.length(), 0) < 0) {
            break;
        }
    }
    close(clientFd);
}
