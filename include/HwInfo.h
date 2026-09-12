#pragma once
#include <Arduino.h>

/// I2C bus pins. These default to whatever the selected board's Arduino variant declares -- 21/22
/// on the classic ESP32, 8/9 on the C3 SuperMini -- so they stay correct when switching between
/// the two envs without editing anything here. Override with literals if wired differently.
/// Note GPIO22 does not exist on the C3 at all, so a hardcoded 21/22 would be a silent break.
#define I2C_SDA_PIN SDA
#define I2C_SCL_PIN SCL

/// 7-bit address of the PCF8574 expander (0x27 is the usual 16x2 LCD backpack strapping).
#define PCF8574_ADDRESS 0x27

/// Pins below are PCF8574 port bits (0-7), not GPIOs.
#define BUTTON_1_PIN 0
#define BUTTON_1_LIGHT_PIN 1
#define LIGHT_RELAY_PIN 3
#define DOOR_SENSOR_PIN 4
