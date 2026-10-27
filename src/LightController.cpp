#include "LightController.h"
#include <NightMare.h>
#include <NightMare/Config.h>
#include <IoXpander.h>

static Config<float> onVoltageThreshold("light:on_voltage_threshold",
                                        ZMPT101B_ON_VOLTAGE_THRESHOLD);
static Config<uint32_t> samplePeriodMs("light:sample_period_ms", SAMPLE_PERIOD_MS);
static Config<uint32_t> settleMs("light:settle_ms", LIGHT_SETTLE_MS);

static ManagedState<bool> light("light");

static TaskHandle_t lightTaskHandle = nullptr;
static int zeroPoint = -1;
static volatile bool lastState = false;
static volatile bool holdActive = false;
static volatile uint32_t holdStartMs = 0;

bool currentLightState()
{
    return lastState;
}

bool lightToggle()
{
    return IoXpander.togglePin(LIGHT_RELAY_PIN);
}

// ---- resource handlers --------------------------------------------------------------------------

/// The relay is toggled rather than driven to a level: the measured mains state, not the relay
/// latch, says whether the light is on. Accepting commits the requested value right away and the
/// sampler holds it for light:settle_ms before the measurement takes over again.
static bool lightOnWrite(ManagedState<bool> &, const bool &requested)
{
    if (requested != lastState && !lightToggle())
        return false;
    holdStartMs = millis();
    holdActive = true;
    return true;
}

void lightOnButton(byte current, byte previous)
{
    const byte pressed = (current ^ previous) & current & (1 << BUTTON_1_PIN);
    if (pressed)
        lightToggle();
}

// ---- sampling -----------------------------------------------------------------------------------

static int getZeroPoint()
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

static float fastSensorRead()
{
    int32_t Vnow = 0;
    uint32_t Vsum = 0;
    uint32_t measurements_count = 0;
    uint32_t t_start = micros();

    while (micros() - t_start < ZMPT101B_PERIOD_MICROSECONDS)
    {
        Vnow = analogRead(ZMPT101B_PIN) - zeroPoint;
        Vsum += _abs(Vnow);
        measurements_count++;
    }

    return (Vsum / measurements_count) / ADC_SCALE * VREF * ZMPT101B_SENSITIVITY;
}

static void updateLightState()
{
    const bool measured = fastSensorRead() > onVoltageThreshold.value();
    if (measured != lastState)
    {
        lastState = measured;
        IoXpander.digitalWrite(BUTTON_1_LIGHT_PIN, !measured); // button LED is lit while the light is off
    }

    if (holdActive)
    {
        if (millis() - holdStartMs < settleMs.value())
            return;
        holdActive = false;
    }
    static bool published = false;
    if (published && light.getValue() == measured)
        return;
    published = light.setValue(measured);
}

/// Each pass busy-waits one full mains period inside fastSensorRead(), which is why this cannot
/// live in loop() or on the Scheduler task: the measurement cannot be broken up.
static void lightTask(void *)
{
    for (;;)
    {
        uint32_t startMs = millis();
        updateLightState();
        uint32_t elapsed = millis() - startMs;

        // Always yield at least one tick, or the idle task on this core is starved until the task
        // watchdog fires (a sample alone takes longer than a short period).
        const uint32_t period = samplePeriodMs.value();
        uint32_t wait = (elapsed >= period) ? 1 : (period - elapsed);
        TickType_t ticks = pdMS_TO_TICKS(wait);
        bool is_ota_running = SystemState.get(SystemFlag::OtaRunning);
        vTaskDelay(is_ota_running ? 5000 : (ticks ? ticks : 1));
    }
}

void lightStart()
{
    if (lightTaskHandle)
        return; // already running

    pinMode(ZMPT101B_PIN, ANALOG);
    zeroPoint = getZeroPoint();
    Serial.printf("Zero point: %d\n", zeroPoint);

    light.onWrite = lightOnWrite;

#if defined(CONFIG_FREERTOS_UNICORE) || (portNUM_PROCESSORS == 1)
    // C3 SuperMini: one core, so everything shares it. The task still helps -- the busy-wait
    // becomes preemptible instead of monopolising loop().
    BaseType_t ok = xTaskCreate(lightTask, "light_ctrl", LIGHT_TASK_STACK, nullptr,
                                LIGHT_TASK_PRIORITY, &lightTaskHandle);
#else
    // Classic ESP32: the Arduino loop task is pinned to core 1, so sample on core 0 to keep loop()
    // free. Core 0 also carries WiFi, which preempts mid-sample; fastSensorRead() averages over
    // whatever it collects.
    BaseType_t ok = xTaskCreatePinnedToCore(lightTask, "light_ctrl", LIGHT_TASK_STACK, nullptr,
                                            LIGHT_TASK_PRIORITY, &lightTaskHandle, 0);
#endif
    if (ok != pdPASS)
    {
        lightTaskHandle = nullptr;
        Serial.println("LightController: failed to create sampling task");
    }
}
