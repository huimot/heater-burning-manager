#pragma once
#include "IPin.h"
#include <modbus.h>
#include <memory>

// Этот класс знает только свой регистр/бит. Контекст modbus_t ему передается снаружи.
class ModbusPin : public IPin {
public:
    ModbusPin(const std::string& name, PinDirection dir, modbus_t* ctx, int bitAddress, bool inverted = false)
        : m_name(name), m_dir(dir), m_ctx(ctx), m_address(bitAddress), m_inverted(inverted) {}

    bool init() override { return m_ctx != nullptr; }

    bool read() override {
        if (!m_ctx || m_dir != PinDirection::INPUT_PIN) return false;
        uint8_t bit = 0;
        // Читаем прямо из физического блока (или из общего буфера, если делать кэширование)
        if (modbus_read_input_bits(m_ctx, m_address, 1, &bit) == -1) {
            return false; // Ошибка связи
        }
        return m_inverted ? !bit : bit;
    }

    void write(bool state) override {
        if (!m_ctx || m_dir != PinDirection::OUTPUT_PIN) return;
        uint8_t bit = (state ^ m_inverted) ? 1 : 0;
        modbus_write_bit(m_ctx, m_address, bit);
    }

    std::string getName() const override { return m_name; }
    PinDirection getDirection() const override { return m_dir; }

private:
    std::string m_name;
    PinDirection m_dir;
    modbus_t* m_ctx;
    int m_address;
    bool m_inverted;
};
