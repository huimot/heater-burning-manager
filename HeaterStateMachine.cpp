#include "HeaterStateMachine.h"
#include "ConfigAndHardwareManager.h"
#include "Logger.h"
#include <chrono>

HeaterStateMachine::HeaterStateMachine() {
    m_status = HeaterStatus::IDLE;
    m_fHandjob = false;
    m_fAutoEnabled = true;
    m_isHeatEnabled = false;
    m_ignitionCounter = 0;
    m_stateTimer = std::chrono::steady_clock::now();
}

void HeaterStateMachine::setCommand(const std::string& cmd) {
    if (cmd == "heat")     { m_fHandjob = true;  LOG("Command 'heat' received."); }
    if (cmd == "stop")     { m_fHandjob = false; LOG("Command 'stop' received."); }
    if (cmd == "auto")     { m_fAutoEnabled = true;  LOG("Auto mode ENABLED."); }
    if (cmd == "stopauto") { m_fAutoEnabled = false; LOG("Auto mode DISABLED."); }
    if (cmd == "clearerror" && m_status == HeaterStatus::ERROR) {
        LOG("Error cleared. Resetting to IDLE.");
        changeStatus(HeaterStatus::IDLE);
    }
}

HeaterStatus HeaterStateMachine::getStatus() const {
    return m_status;
}

void HeaterStateMachine::changeStatus(HeaterStatus next) {
    if (m_status != next) {
        m_status = next;
        m_stateTimer = std::chrono::steady_clock::now();
        LOG("Status changed to: " + std::to_string(static_cast<int>(next)));
    }
}

void HeaterStateMachine::handleEmergency() {
    changeStatus(HeaterStatus::ERROR);
    LOG("!!! EMERGENCY STOP!!! Turning off all outputs.");

    // Гасим абсолютно всё железо во всех блоках/драйверах
    auto& hw = HardwareManager::instance();
    hw.getPin("FuelHeater")->write(false);
    hw.getPin("Gate")->write(false);
    hw.getPin("Blower")->write(false);
    hw.getPin("HeaterAccepted")->write(false);
    hw.getPin("Ignition")->write(false);
}

void HeaterStateMachine::process() {
    auto& hw = HardwareManager::instance();
    auto now = std::chrono::steady_clock::now();

    // 1. Получаем ссылки на все наши абстрактные пины
    auto pinOverheat   = hw.getPin("OverHeat");
    auto pinExtCmd     = hw.getPin("HeatExtCmd");
    auto pinFuelHeated = hw.getPin("FuelHeated");
    auto pinFlame      = hw.getPin("FlameDetected");

    auto pinFuelHeater = hw.getPin("FuelHeater");
    auto pinGate       = hw.getPin("Gate");
    auto pinBlower     = hw.getPin("Blower");
    auto pinAccepted   = hw.getPin("HeaterAccepted");
    auto pinIgnition   = hw.getPin("Ignition");

    // 2. Читаем датчики безопасности
    bool isOverheat = pinOverheat->read();
    bool isExtCmd   = pinExtCmd->read() && m_fAutoEnabled;

    // Общая логика разрешения работы (учитывает термостат и внешнюю команду)
    m_isHeatEnabled = (m_fHandjob || isExtCmd) && !isOverheat;

    // Аппаратный Watchdog / Защита по перегреву
    if (isOverheat && m_status != HeaterStatus::ERROR) {
        LOG("CRITICAL: Overheat sensor triggered!");
        handleEmergency();
        return;
    }

    // Лимиты из конфига (с дефолтами, если в json пусто)
    int tAfterHeating = hw.getTimeout("after_heating", 0); // Твой коммент: "wait even 0 minutes"
    int tGateOpen     = hw.getTimeout("gate_open", 18);
    int tBlowingUp    = hw.getTimeout("blowing_up", 15);
    int tBlowingDown  = hw.getTimeout("blowing_down", 240);
    int tGateClose    = hw.getTimeout("gate_close", 18);

    // 3. Поехали по состояниям без блокировок
    switch (m_status) {
    case HeaterStatus::IDLE:
        // Всё выключено, ждем команду на запуск
        if (m_isHeatEnabled) {
            LOG("Heating requested. Turning FuelHeater ON.");
            pinFuelHeater->write(true);
            changeStatus(HeaterStatus::FUELHEATING);
        } else {
            // На всякий случай держим выходы выключенными в IDLE
            pinFuelHeater->write(false);
            pinGate->write(false);
            pinBlower->write(false);
            pinAccepted->write(false);
            pinIgnition->write(false);
        }
        break;

    case HeaterStatus::FUELHEATING:
        if (!m_isHeatEnabled) { changeStatus(HeaterStatus::IDLE); break; }
        if (pinFuelHeated->read()) {
            LOG("Fuel is hot. Moving to after-heating delay.");
            changeStatus(HeaterStatus::FUELAFTERHEATING);
        }
        break;

    case HeaterStatus::FUELAFTERHEATING:
        if (!m_isHeatEnabled) { changeStatus(HeaterStatus::IDLE); break; }
        if (now - m_stateTimer >= std::chrono::seconds(tAfterHeating)) {
            LOG("After-heating complete. Opening the gate.");
            pinGate->write(true);
            changeStatus(HeaterStatus::GATEOPENING);
        }
        break;

    case HeaterStatus::GATEOPENING:
        if (now - m_stateTimer >= std::chrono::seconds(tGateOpen)) {
            LOG("Gate is fully open. Turning Blower ON.");
            pinBlower->write(true);
            m_ignitionCounter = 0;
            changeStatus(HeaterStatus::GATEOPEN);
        }
        break;

    case HeaterStatus::GATEOPEN:
        changeStatus(HeaterStatus::BLOWINGUP);
        break;

    case HeaterStatus::BLOWINGUP:
        if (m_ignitionCounter > 16) { // MAX_IGNITIONS
            LOG("Error: Too many ignition attempts!");
            handleEmergency();
            break;
        }
        if (now - m_stateTimer >= std::chrono::seconds(tBlowingUp)) {
            m_ignitionCounter++;
            LOG("Blowing up complete. Attempt counter: " + std::to_string(m_ignitionCounter));
            changeStatus(HeaterStatus::IGNITEREADY);
        }
        break;

    case HeaterStatus::IGNITEREADY:
        pinAccepted->write(true);
        // Из старого кода: "isIgniteWithBlower" у тебя стоял false по дефолту
        // Готовимся к поджигу — гасим вентилятор на 2 секунды
        LOG("Ready for ignition. Stopping blower for 2 seconds.");
        pinBlower->write(false);
        changeStatus(HeaterStatus::IGNITING); // Прямой переход, время отсчитаем в следующем кейсе
        break;

    case HeaterStatus::IGNITING:
        // Сначала ждем 2 секунды тишины без вентилятора, потом включаем поджиг
        if (now - m_stateTimer < std::chrono::seconds(2)) {
            break;
        }

        // Включаем трансформатор поджига
        pinIgnition->write(true);

        {
            // Рассчитываем время поджига: 12сек + 0.5сек на каждую неудачную попытку
            int ignition_time_ms = 12000 + m_ignitionCounter * 500;
            // Учитываем 2 секунды (2000мс) начальной паузы вентилятора
            if (now - m_stateTimer >= std::chrono::milliseconds(ignition_time_ms + 2000)) {
                LOG("Stopping ignition transformer. Turning blower back ON.");
                pinIgnition->write(false);
                pinBlower->write(true); // Возвращаем продувку
                changeStatus(HeaterStatus::FLAMEWAITING);
            }
        }
        break;

    case HeaterStatus::FLAMEWAITING:
        // Ищем пламя в течение 5 секунд (в старом коде было 50 циклов usleep по 100мс)
        if (pinFlame->read()) {
            LOG("FLAME DETECTED! Boiler is burning.");
            changeStatus(HeaterStatus::BURNING);
        } else if (now - m_stateTimer >= std::chrono::seconds(5)) {
            LOG("Flame detection FAILED. Going back to blowing up.");
            pinAccepted->write(false);
            changeStatus(HeaterStatus::BLOWINGUP);
        }
        break;

    case HeaterStatus::BURNING:
        if (!m_isHeatEnabled) {
            LOG("Stop command or external limit hit. Moving to AFTERBURNING.");
            pinAccepted->write(false);
            changeStatus(HeaterStatus::AFTERBURNING);
        } else if (!pinFlame->read()) {
            LOG("CRITICAL: Flame broken during burning! Restarting cycle.");
            pinAccepted->write(false);
            changeStatus(HeaterStatus::BLOWINGUP);
        }
        break;

    case HeaterStatus::AFTERBURNING:
        // Твой коммент: "TODO - hz". Переходим сразу на продувку и охлаждение
        changeStatus(HeaterStatus::BLOWINGDOWN);
        break;

    case HeaterStatus::BLOWINGDOWN:
        if (now - m_stateTimer >= std::chrono::seconds(tBlowingDown)) {
            LOG("Blowing down complete. Closing gate.");
            pinBlower->write(false);
            pinGate->write(false);
            changeStatus(HeaterStatus::GATECLOSING);
        }
        break;

    case HeaterStatus::GATECLOSING:
        if (now - m_stateTimer >= std::chrono::seconds(tGateClose)) {
            LOG("Gate closed. Disabling fuel heater. System IDLE.");
            pinFuelHeater->write(false);
            changeStatus(HeaterStatus::IDLE);
        }
        break;

    case HeaterStatus::ERROR:
        // Сидим здесь намертво и держим всё выключенным, пока не прилетит clearerror
        break;
    }
}
