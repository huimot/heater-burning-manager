#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <memory>

// Опережающее объявление (forward declaration) — clangd будет доволен
class HeaterStateMachine;

class TcpControlServer {
public:
    TcpControlServer(int port, std::shared_ptr<HeaterStateMachine> sm);
    ~TcpControlServer();

    void start();
    void stop();

private:
    void serverLoop();
    void handleClient(int clientFd);

    int m_port;
    int m_serverFd;
    std::atomic<bool> m_running;
    std::thread m_serverThread;
    std::shared_ptr<HeaterStateMachine> m_sm;
};
