#include "IoXpander.h"
#include <LOGS.h>
ioXpander IoXpander;

/// Created here rather than lazily on first use: lazy creation would itself race if two tasks
/// happened to call in simultaneously. Static constructors run after the ESP-IDF heap is up, so
/// allocating a FreeRTOS object at this point is fine.
ioXpander::ioXpander()
{
    stateLock = xSemaphoreCreateRecursiveMutex();
}

void ioXpander::lockState()
{
    if (stateLock)
        xSemaphoreTakeRecursive(stateLock, portMAX_DELAY);
}

void ioXpander::unlockState()
{
    if (stateLock)
        xSemaphoreGiveRecursive(stateLock);
}

bool ioXpander::push()
{
    wire->beginTransmission(address);
    wire->write(write_data);
    return wire->endTransmission() == 0;
}

bool ioXpander::begin(int sclPin, int sdaPin, uint8_t address)
{
    this->address = address;
    bool res = wire->begin(sdaPin, sclPin); // Wire takes (sda, scl) -- the reverse of this signature
    if (!res)
        return false;
    // Release every pin before anything else, matching the chip's power-on latch. Doing this first
    // means a stale latch can never drive a pin low against whatever is wired to it, and it also
    // doubles as the presence probe: a device that isn't there won't ACK.
    write_data = 0xFF;
    connected = push();
    if (connected)
        update();
    initialized = connected;
    this->setAsInput(BUTTON_1_PIN);
    this->setAsInput(DOOR_SENSOR_PIN);

    return connected;
}

bool ioXpander::update()
{
    if (!initialized)
        return false;
    lockState();
    if (wire->requestFrom(address, (uint8_t)1) != 1)
    {
        connected = false;
        unlockState();
        return false;
    }
    byte previous = read_data;
    read_data = (byte)wire->read();
    lastUpdate = millis();
    connected = true;

    bool baseline = !hasSampled;
    hasSampled = true;
    byte current = read_data;
    // Decide inside the lock so the snapshot is consistent, fire outside it.
    bool fire = (changeCb && !baseline && current != previous);
    unlockState();

    // Deliberately outside the lock: in this project the callback publishes over MQTT and calls
    // back into this object. Holding the state lock across network I/O would serialise every
    // other caller behind it for no benefit -- the callback gets the snapshot as arguments.
    if (fire)
        changeCb(current, previous);
    return true;
}

void ioXpander::onChange(ChangeCallback callback)
{
    changeCb = callback;
}

void ioXpander::snapshot(byte &level, byte &latch, bool resample)
{
    // Resample before locking, not inside: update() fires the onChange callback, which may write
    // pins. Doing it first means both bytes below are read after any such write, so they agree.
    if (resample)
        update();
    lockState();
    level = read_data;
    latch = write_data;
    unlockState();
}

byte ioXpander::getCurrentData(bool update)
{
    if (update)
        this->update(); // the parameter shadows the method, hence the explicit this->
    return read_data;
}

bool ioXpander::write(byte value)
{
    lockState();
    write_data = value;
    connected = push();
    bool res = connected;
    unlockState();
    return res;
}

bool ioXpander::digitalWrite(uint8_t pin, bool value)
{
    if (pin > 7)
        return false;
    // The read-modify-write below is exactly what the lock exists for: without it, two tasks
    // touching different pins can each read the old latch and the second push wins, silently
    // dropping the first pin change.
    lockState();
    if (value)
        write_data |= (byte)(1 << pin);
    else
        write_data &= (byte) ~(1 << pin);
    connected = push();
    bool res = connected;
    unlockState();
    Serial.printf("[%s] Write to pin %d: %s\n", OK_LOG(res), pin, value ? "HIGH" : "LOW");

    return res;
}

bool ioXpander::digitalRead(uint8_t pin, bool update)
{
    if (pin > 7)
        return false;
    if (update)
        this->update();
    return ((read_data >> pin) & 0x01) != 0;
}

bool ioXpander::togglePin(uint8_t pin)
{
    if (pin > 7)
        return false;
    lockState();
    write_data ^= (byte)(1 << pin);
    connected = push();
    bool res = connected;
    unlockState();
    Serial.printf("[%s] Toggled pin %d\n", OK_LOG(res), pin);
    return res;
}

bool ioXpander::setAsInput(uint8_t pin)
{
    // There is no direction register: "input" just means leaving the latch bit high so an external
    // driver can pull the pin down against the weak internal pull-up.
    return digitalWrite(pin, true);
}

/// @brief Names the chip family an address belongs to, to shortcut the usual "which variant is
/// this board?" guessing. Ranges only -- an address in range is a hint, not a identification.
static const char *addressHint(uint8_t addr)
{
    if (addr >= 0x20 && addr <= 0x27)
        return "  <- PCF8574 range (0x27 = the usual 16x2 LCD backpack)";
    if (addr >= 0x38 && addr <= 0x3F)
    {
        // 0x3C/0x3D sit in the PCF8574A range but are far more often an SSD1306 OLED.
        if (addr == 0x3C || addr == 0x3D)
            return "  <- PCF8574A range, but usually an SSD1306 OLED";
        return "  <- PCF8574A range (0x3F = the usual 16x2 LCD backpack)";
    }
    return "";
}
String ioXpander::scanBusJson()
{
    String out = "{\n";
    out += "  \"scan\": [\n";

    uint8_t found = 0;
    for (uint16_t addr = 0; addr <= 127; addr++)
    {
        wire->beginTransmission((uint8_t)addr);
        // 0 means the device ACKed its address. Anything else is NACK, bus error or timeout, all
        // of which mean "nothing usable here".
        if (wire->endTransmission() != 0)
            continue;
        found++;
        out += "    {\"address\": \"0x";
        char buf[16];
        snprintf(buf, sizeof(buf), "%02X", (uint8_t)addr);
        out += buf;
        out += "\", \"hint\": \"";
        out += addressHint((uint8_t)addr);
        out += "\"}";
        if (found < 127)
            out += ",";
        out += "\n";
    }

    out += "  ]\n";
    out += "}\n";
    return out;
}
String ioXpander::scanBus(uint8_t from, uint8_t to)
{
    String out = "I2C scan 0x";
    char buf[96];
    snprintf(buf, sizeof(buf), "%02X..0x%02X\n", from, to);
    out += buf;

    uint8_t found = 0;
    for (uint16_t addr = from; addr <= to; addr++)
    {
        wire->beginTransmission((uint8_t)addr);
        // 0 means the device ACKed its address. Anything else is NACK, bus error or timeout, all
        // of which mean "nothing usable here".
        if (wire->endTransmission() != 0)
            continue;
        found++;
        snprintf(buf, sizeof(buf), "  0x%02X%s\n", (uint8_t)addr, addressHint((uint8_t)addr));
        out += buf;
    }

    if (found == 0)
        out += "  no devices responded - check SDA/SCL wiring, pull-ups and power\n";
    else
    {
        snprintf(buf, sizeof(buf), "  %u device(s) found\n", found);
        out += buf;
    }
    return out;
}

String ioXpander::getStateString()
{
    char buf[128];
    String out = "PCF8574 @ 0x";
    snprintf(buf, sizeof(buf), "%02X  %s", address, connected ? "connected" : "NOT RESPONDING");
    out += buf;

    if (hasSampled)
    {
        snprintf(buf, sizeof(buf), ", sampled %lums ago\n", (unsigned long)(millis() - lastUpdate));
        out += buf;
    }
    else
        out += ", never sampled\n";

    out += " pin  latch  read\n";
    for (uint8_t pin = 0; pin < 8; pin++)
    {
        bool latch = (write_data >> pin) & 0x01;
        bool level = (read_data >> pin) & 0x01;
        const char *note = "";
        if (latch && !level)
            note = "  pulled low externally"; // the normal "input is active" case
        else if (!latch && level)
            note = "  DRIVEN LOW BUT READS HIGH - shorted to Vcc?";
        snprintf(buf, sizeof(buf), "   %u      %u     %u%s\n", pin, latch ? 1 : 0, level ? 1 : 0, note);
        out += buf;
    }

    snprintf(buf, sizeof(buf), " latch=0x%02X  read=0x%02X\n", write_data, read_data);
    out += buf;
    return out;
}

void ioXpander::printState(Print &out)
{
    out.print(getStateString());
}
