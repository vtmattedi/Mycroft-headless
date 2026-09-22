#pragma once

#include <NightMare/HardwareProfile.h>
#include <HwInfo.h>
#include <LightController.h>
#include <TempSensor.h>

// What INFO / telemetry report as this board's physical connections. Only real GPIOs are listed;
// the button, door switch and relay hang off the PCF8574, so they are described on the I2C bus.
namespace NMHardware
{
inline Profile projectProfile()
{
    static const Connection connections[] = {
        {"I2C SDA", I2C_SDA_PIN, Direction::Bus, Pull::External, false,
         "PCF8574 @0x27: P0 button, P1 button LED, P3 light relay, P4 door reed switch"},
        {"I2C SCL", I2C_SCL_PIN, Direction::Bus, Pull::External, false, "PCF8574 clock"},
        {"ZMPT101B", ZMPT101B_PIN, Direction::Input, Pull::None, false, "mains voltage sense for light state"},
        {"DS18B20", DS18B20_PIN, Direction::Bidirectional, Pull::External, false, "1-Wire; 4.7k pull-up to 3V3"},
    };
    return {
#ifdef ESP32_C3
        "ESP32-C3 SuperMini",
#else
        "ESP32 DevKit v1",
#endif
        connections, sizeof(connections) / sizeof(connections[0])};
}
}
