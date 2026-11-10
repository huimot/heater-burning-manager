#pragma once
#include <string>

enum class PinDirection { INPUT_PIN, OUTPUT_PIN };

class IPin {
public:
    virtual ~IPin() = default;
    virtual bool init() = 0;
    
    // Для входных пинов
    virtual bool read() = 0;
    
    // Для выходных пинов
    virtual void write(bool state) = 0;
    
    virtual std::string getName() const = 0;
    virtual PinDirection getDirection() const = 0;
};
