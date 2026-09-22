#include "LightController.h"
#include <NightMare.h>
static void (*onLightStateChangeCallback)(bool,float) = NULL;
static int zeroPoint = -1;
static bool last_state = false;
int getZeroPoint()
{
    uint32_t Vsum = 0;
    uint32_t measurements_count = 0;
    uint32_t t_start = micros();

    while (micros() - t_start < ZMPT101B_PERIOD_MICROSECONDS)
    {
        Vsum += analogRead(ZMPT101B_PIN);
        measurements_count++;
    }

    return Vsum / measurements_count;
}

static TaskHandle_t lightTaskHandle = nullptr;

/// @brief Samples forever on its own task.
/// Each pass busy-waits one full mains period inside fastSensorRead(), which is why this cannot
/// live in loop(): there is no way to break the measurement up without ruining it.
static void lightControllerTask(void *)
{
    for (;;)
    {
        uint32_t startMs = millis();
        updateLightState();
        uint32_t elapsed = millis() - startMs;

        // Always yield at least one tick. A sample busy-waits ~16.7 ms at 60 Hz, so whenever
        // SAMPLE_PERIOD_MS is shorter than that a plain fixed-rate delay would compute a zero
        // wait, this task would never block, and the idle task on this core would be starved
        // until the task watchdog fired.
        uint32_t wait = (elapsed >= SAMPLE_PERIOD_MS) ? 1 : (SAMPLE_PERIOD_MS - elapsed);
        TickType_t ticks = pdMS_TO_TICKS(wait);
        bool is_ota_running = SystemState.getFlag("ota_running");
        vTaskDelay(is_ota_running ? 5000 : (ticks ? ticks : 1));
    }
}

void setupLightController()
{
    pinMode(ZMPT101B_PIN, ANALOG);
    if (zeroPoint == -1)
    {
        zeroPoint = getZeroPoint();
        Serial.printf("Zero point: %d\n", zeroPoint);
    }
    if (lightTaskHandle)
        return; // already running

#if defined(CONFIG_FREERTOS_UNICORE) || (portNUM_PROCESSORS == 1)
    // C3 SuperMini: one core, so everything shares it. The task still helps -- the busy-wait
    // becomes preemptible instead of monopolising loop().
    BaseType_t ok = xTaskCreate(lightControllerTask, "light_ctrl", LIGHT_TASK_STACK, nullptr,
                                LIGHT_TASK_PRIORITY, &lightTaskHandle);
#else
    // Classic ESP32: the Arduino loop task is pinned to core 1, so put sampling on core 0 to keep
    // loop() free. Core 0 also carries the WiFi stack, which will preempt mid-sample; that costs
    // some sample density but fastSensorRead() averages over whatever it manages to collect.
    BaseType_t ok = xTaskCreatePinnedToCore(lightControllerTask, "light_ctrl", LIGHT_TASK_STACK,
                                            nullptr, LIGHT_TASK_PRIORITY, &lightTaskHandle, 0);
#endif
    if (ok != pdPASS)
    {
        lightTaskHandle = nullptr;
        Serial.println("LightController: failed to create sampling task");
    }
}

float fastSensorRead()
{
    double readingVoltage = 0.0f;

    int32_t Vnow = 0;
    uint32_t Vsum = 0;
    uint32_t measurements_count = 0;
    uint32_t t_start = micros();

    while (micros() - t_start < ZMPT101B_PERIOD_MICROSECONDS )
    {
        Vnow = analogRead(ZMPT101B_PIN) - zeroPoint;
        Vsum += _abs(Vnow);
        measurements_count++;
    }

    readingVoltage = (Vsum / measurements_count) / ADC_SCALE * VREF * ZMPT101B_SENSITIVITY;

    return readingVoltage;
}

void onLightStateChange(void (*callback)(bool newState, float sensorValue))
{
    onLightStateChangeCallback = callback;
}

void updateLightState()
{
    float rawRead = fastSensorRead();
    // Serial.printf("Light sensor reading: %.2f\n", rawRead);
    bool current_state = rawRead > ZMPT101B_ON_VOLTAGE_THRESHOLD;
    if (current_state != last_state)
    {
        last_state = current_state;   
        if (onLightStateChangeCallback)
        {
            onLightStateChangeCallback(current_state, rawRead);
        }
    }
}

bool currentLightState()
{
    return last_state;
}