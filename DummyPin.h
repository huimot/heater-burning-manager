class DummyPin : public IPin {
public:
    DummyPin(const std::string& name, PinDirection dir) : m_name(name), m_dir(dir), m_state(false) {}
    bool init() override { return true; }
    bool read() override { return m_state; }
    void write(bool state) override { m_state = state; }
    std::string getName() const override { return m_name; }
    PinDirection getDirection() const override { return m_dir; }
private:
    std::string m_name;
    PinDirection m_dir;
    bool m_state;
};
