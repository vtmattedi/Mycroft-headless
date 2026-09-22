#pragma once
#include <Arduino.h>
#define ZMPT101B_SENSITIVITY 673.625
#ifdef ESP32_C3
#define ZMPT101B_PIN 3
#else
#define ZMPT101B_PIN 32
#endif
#define ZMPT101B_FREQUENCY 60.0
#define ZMPT101B_PERIOD_MICROSECONDS (1000000 / ZMPT101B_FREQUENCY)
#define ZMPT101B_ON_VOLTAGE_THRESHOLD 65.0
#define ADC_SCALE 4095.0
#define VREF 3.3
/// Gap between samples. Note a single sample busy-waits one full mains period
/// (ZMPT101B_PERIOD_MICROSECONDS, ~16.7 ms at 60 Hz), so any value below that means the sampler
/// runs effectively back-to-back at ~100% duty on its core.
#define SAMPLE_PERIOD_MS 5

/// Sampling task. The stack has to cover the onLightStateChange callback, if one is registered.
#define LIGHT_TASK_STACK 4096
/// Above the Arduino loop task (priority 1), far below the WiFi stack (18+).
#define LIGHT_TASK_PRIORITY 1
bool currentLightState();
void updateLightState();
void setupLightController();
void onLightStateChange(void (*callback)(bool newState, float sensorValue));