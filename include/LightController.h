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
/// After a requested change the resource holds the requested value for this long, waiting for the
/// measured mains state to follow; if it never does, the measurement wins. Covers relay travel plus
/// a few ZMPT101B samples (~17 ms each at 60 Hz).
#define LIGHT_SETTLE_MS 1500

/// Sampling task. The stack has to cover the resource publish it triggers.
#define LIGHT_TASK_STACK 4096
/// Above the Arduino loop task (priority 1), far below the WiFi stack (18+).
#define LIGHT_TASK_PRIORITY 1

/// Room light: the mains sense (ZMPT101B) is the state, the relay on the IO expander the actuator.
/// Resources: light (state, writable), toggle_light (action).
/// Configs: light:on_voltage_threshold, light:sample_period_ms, light:settle_ms.

/// @brief Starts the sampling task. Call after ioXpanderStart().
void lightStart();
/// @brief The measured light state, as of the last sample.
bool currentLightState();
/// @brief Flips the relay. @return False when the expander did not ACK.
bool lightToggle();
/// @brief Feeds an expander change to the light: a press of the wall button toggles it.
void lightOnButton(byte current, byte previous);
