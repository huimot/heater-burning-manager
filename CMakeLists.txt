cmake_minimum_required(VERSION 3.14)
project(heater_daemon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск системной библиотеки libmodbus
find_package(PkgConfig REQUIRED)
pkg_check_modules(MODBUS REQUIRED libmodbus)

# Поиск nlohmann_json
find_package(nlohmann_json REQUIRED)

# Добавляем все наши исходники (создайте эти файлы в папке проекта)
add_executable(heater_daemon
    main.cpp
    IPin.h
    ModbusPin.h
    DummyPin.h
    ConfigAndHardwareManager.h
    HeaterStateMachine.h
    TcpControlServer.h
    Logger.h
)

# Подключаем пути к заголовочным файлам и линкуем библиотеки
target_include_directories(heater_daemon PRIVATE ${MODBUS_INCLUDE_DIRS})
target_link_libraries(heater_daemon PRIVATE ${MODBUS_LIBRARIES} nlohmann_json::nlohmann_json)
