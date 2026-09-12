#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <HwInfo.h>

/// 7-bit I2C address of a PCF8574 with A0..A2 tied low.
/// The pin-compatible PCF8574**A** bases at 0x38 instead. The 16x2 LCD backpacks are the same chip
/// strapped differently, usually 0x27 (PCF8574T) or 0x3F (PCF8574AT) -- so a board pulled off an
/// LCD will most likely need one of those passed to begin() rather than this default.
#define IOXPANDER_DEFAULT_ADDRESS PCF8574_ADDRESS

/// @brief Driver for a PCF8574 8-bit I2C I/O expander.
///
/// The PCF8574 has no direction register. Every pin is a weak (~100 uA) current-source pull-up in
/// parallel with a strong pull-down transistor -- what the datasheet calls quasi-bidirectional:
///   - writing 0 drives the pin hard low
///   - writing 1 releases it, leaving only the weak pull-up holding it high
///
/// A pin is therefore usable as an input only while its latch bit is 1, since that is the only
/// state an external driver (a button to GND, an open-drain sensor) can override. Reads return the
/// actual pin levels rather than the latch, so a pin you wrote 1 to reads back 0 the whole time
/// something is pulling it down. That is the mechanism, not a fault.
///
/// Two consequences worth knowing before wiring anything:
///   - The latch starts at 0xFF, matching the chip's own power-on state, so nothing is driven low
///     until you ask for it. Call setAsInput() for any pin you intend to read.
///   - The pull-up is weak. It will hold a CMOS input or an LED-to-ground (dimly), but it cannot
///     source real current -- drive loads to ground with the pin low, not to Vcc with it high.
///
/// The chip also has an open-drain INT output that pulses on any input change, which can replace
/// polling entirely; this driver polls via update() and does not use it.
class ioXpander
{
public:
    /// @brief Signature for the change callback.
    /// @param current Freshly sampled pin levels. @param previous The levels before this sample.
    /// `current ^ previous` is the mask of pins that moved.
    typedef void (*ChangeCallback)(byte current, byte previous);

private:
    bool initialized = false;
    unsigned long lastUpdate = 0;
    TwoWire *wire = &Wire;
    /// Output latch. 1 = released (weak pull-up, readable), 0 = driven low.
    /// Starts at 0xFF so a fresh object never drives a pin low before begin() decides to.
    byte write_data = 0xFF;
    /// Pin levels as of the last update(), which is not necessarily what was written.
    byte read_data = 0xFF;
    uint8_t address = IOXPANDER_DEFAULT_ADDRESS;
    bool connected = false;
    ChangeCallback changeCb = nullptr;
    /// False until the first successful sample. The baseline read is not a change, so it must not
    /// fire the callback -- otherwise every boot reports a spurious event for each pin that
    /// happens not to match read_data's initial 0xFF.
    bool hasSampled = false;

    /// Guards this object's own state, which is not the same thing as guarding the bus.
    /// TwoWire is already thread-safe: it takes its mutex in beginTransmission() and holds it
    /// until endTransmission(), and requestFrom() guards itself. What is unprotected is
    /// write_data, which digitalWrite()/togglePin() read-modify-write -- two tasks doing that at
    /// once silently lose one of the updates. Recursive because the public methods call each
    /// other (setAsInput -> digitalWrite, digitalRead(pin, true) -> update).
    SemaphoreHandle_t stateLock = nullptr;
    void lockState();
    void unlockState();

    /// Pushes write_data to the port. Caller must hold stateLock.
    /// @return True if the device ACKed.
    bool push();

public:
    ioXpander();

    /// @brief Starts the I2C bus and confirms the expander answers.
    /// @param sclPin Clock pin. @param sdaPin Data pin. Note this is the opposite order to
    /// Wire::begin(), and that the defaults follow the selected board rather than being hardcoded.
    /// (They are named *Pin rather than SCL/SDA because a parameter called SCL would shadow the
    /// variant's SCL constant, making the default argument `SCL = SCL` self-referential.)
    /// @param address 7-bit address; see IOXPANDER_DEFAULT_ADDRESS for the variants.
    /// @return True if the device ACKed. False means absent, miswired, or the wrong address --
    /// worth checking, since a missing expander otherwise just reads as "all pins high".
    bool begin(int sclPin = I2C_SCL_PIN, int sdaPin = I2C_SDA_PIN, uint8_t address = IOXPANDER_DEFAULT_ADDRESS);

    /// @brief Samples all eight pins into the cached read_data.
    /// @return True on a successful read.
    bool update();

    /// @brief Returns the cached pin levels.
    /// @param update True to sample first; false to return the last sample.
    byte getCurrentData(bool update = false);

    /// @brief Replaces the whole output latch. Bits set to 0 are driven low.
    bool write(byte value);

    /// @brief Sets one pin of the latch. @param pin 0-7. @param value true = released, false = low.
    bool digitalWrite(uint8_t pin, bool value);

    /// @brief Reads one pin. @param pin 0-7. @param update True to sample first.
    bool digitalRead(uint8_t pin, bool update = false);
    
    /// @brief Toggles the state of a pin. @param pin 0-7.
    bool togglePin(uint8_t pin);

    /// @brief Releases a pin so it can be read -- the PCF8574's substitute for pinMode(INPUT).
    bool setAsInput(uint8_t pin);

    /// @brief Registers a callback fired by update() whenever the sampled levels differ from the
    /// previous sample. Pass nullptr to unregister.
    /// The first successful sample establishes a baseline and does not fire -- it is not a change.
    /// The callback runs inside update(), on whatever task called it, so keep it short and do not
    /// call update() from within it.
    void onChange(ChangeCallback callback);

    /// @brief Probes every valid 7-bit address on the bus and reports what answered.
    /// Use this when the expander doesn't respond: the LCD backpacks are strapped to 0x27 or 0x3F
    /// rather than the bare chip's 0x20, and this is the quickest way to find out which you have.
    /// Requires the bus to be up, so call begin() first (even if it failed).
    /// @param from First address to probe. @param to Last address to probe.
    /// 0x00-0x07 and 0x78-0x7F are reserved by the I2C spec and skipped by default.
    /// @return A human-readable report, one line per device found.
    String scanBus(uint8_t from = 0x08, uint8_t to = 0x77);

    /// @brief Probes every valid 7-bit address on the bus and reports what answered in JSON format.
    /// @return A JSON string with the scan results.
    String scanBusJson();

    /// @brief Renders the latch bit and the sampled level for all eight pins, flagging any pin
    /// whose two disagree. Does not sample -- call update() first if you want fresh data.
    String getStateString();

    /// @brief Convenience wrapper that prints getStateString() straight to a stream.
    void printState(Print &out = Serial);

    bool isConnected() const { return connected; }
    /// The latch we last wrote, as opposed to getCurrentData()'s actual pin levels.
    byte getLatchedData() const { return write_data; }

    /// @brief Takes the sampled levels and the commanded latch together, under one lock.
    /// Fetching them with two separate calls can straddle a change: update() fires the onChange
    /// callback, and in this project that callback writes pins -- so the latch can move between
    /// the two reads and the report would mix two different instants.
    /// @param level Receives the actual pin levels. @param latch Receives the commanded latch.
    /// @param resample True to sample the port before taking the snapshot.
    void snapshot(byte &level, byte &latch, bool resample = false);
    /// millis() at the last successful update(), for age-based polling.
    unsigned long getLastUpdate() const { return lastUpdate; }
};

extern ioXpander IoXpander;
