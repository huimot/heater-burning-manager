#pragma once
#include <chrono>
#include <atomic>
#include <string>

enum class HeaterStatus {
    IDLE, FUELHEATING, FUELAFTERHEATING, GATEOPENING, GATEOPEN, 
    BLOWINGUP, IGNITEREADY, IGNITING, FLAMEWAITING, BURNING, 
    AFTERBURNING, BLOWINGDOWN, GATECLOSING, ERROR
};

class HeaterStateMachine {
public:
    HeaterStateMachine();
    ~HeaterStateMachine() = default;

    void setCommand(const std::string& cmd);
    HeaterStatus getStatus() const;
    void process();

private:
    void changeStatus(HeaterStatus next);
    void handleEmergency();

    HeaterStatus m_status;
    std::atomic<bool> m_fHandjob{false};
    std::atomic<bool> m_fAutoEnabled{true};
    bool m_isHeatEnabled{false};
    int m_ignitionCounter{0};
    std::chrono::steady_clock::time_point m_stateTimer;
};
