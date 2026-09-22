#include <Arduino.h>
#include <ArduinoJson.h>
#include <NightMare.h>
#include <IoXpander.h>
#include <LightController.h>
#include <TempSensor.h>

// Mycroft-headless: room light, door and temperature node.
//   light        ZMPT101B mains sense (the state) + relay on the PCF8574 (the actuator)
//   door         reed switch on the PCF8574
//   temperature  DS18B20
//
// Threading: the resource layer has no locks, and MQTT ingress runs on the MQTT task. So every
// Managed*.setValue() happens in loop(); the sampling tasks and the MQTT-side handlers only touch
// hardware (IoXpander locks itself) and flags that loop() picks up.

/// After a remote /set flips the relay, how long the measured state may lag before it overrides the
/// accepted value. Covers relay travel plus a few ZMPT101B samples (~17 ms each at 60 Hz).
#define LIGHT_SETTLE_MS 1500
/// Period of the legacy "<device>/sensors" report. The Dashboard marks the light stale after 10 min
/// without one.
#define SENSORS_REPORT_INTERVAL_MS 60000
/// The expander's INT line is not wired, so it is polled -- at the rate the old Timers job used.
#define IOXPANDER_POLL_MS 1

// ---- resources ---------------------------------------------------------------------------------

static ManagedState<bool> light("light");
static ManagedSensor<bool> door("door");
static ManagedSensor<float> temperature("temperature");
static ManagedSensor<bool> temperatureConnected("temperature_connected");
static ManagedAction toggleLight("toggle_light");

/// Set on the MQTT task by onLightWrite(), cleared by loop() once LIGHT_SETTLE_MS has passed.
static volatile bool lightHoldActive = false;
static volatile uint32_t lightHoldStartMs = 0;
/// Set on any task that wants a legacy report out; loop() sends it once MQTT is up.
static volatile bool sensorsReportPending = true;

/// @brief Moves the light towards the requested state. The relay is toggled rather than driven to a
/// level, as the original firmware did: the measured mains state, not the relay latch, says whether
/// the light is on.
/// @return False only when the expander did not ACK the toggle.
static bool driveLight(bool on)
{
    if (on == currentLightState())
        return true;
    return IoXpander.togglePin(LIGHT_RELAY_PIN);
}

static bool isDoorOpen(byte level)
{
    return (level >> DOOR_SENSOR_PIN) & 1;
}

/// @brief Remote /set on the light (MQTT task, or loop() for a serial "> light set").
/// Accepting commits the requested value right away; syncLight() holds off for LIGHT_SETTLE_MS and
/// then reverts it to the measured state if the light never followed.
static bool onLightWrite(ManagedState<bool> &, const bool &requested)
{
    if (!driveLight(requested))
        return false;
    lightHoldStartMs = millis();
    lightHoldActive = true;
    return true;
}

static ActionResult onToggleLight(ManagedAction &, const String &payload)
{
    if (payload.length() != 0)
        return {false, "toggle_light takes no arguments"};
    if (!IoXpander.togglePin(LIGHT_RELAY_PIN))
        return {false, "IO expander did not respond"};
    // No setValue here: the light resource follows the measurement in syncLight().
    return {true, "OK"};
}

static bool bindResources()
{
    light.onWrite = onLightWrite;
    toggleLight.onInvoke = onToggleLight;

    bool ok = gResourcesManager.bindResource(&light);
    ok = gResourcesManager.bindResource(&door) && ok;
    ok = gResourcesManager.bindResource(&temperature) && ok;
    ok = gResourcesManager.bindResource(&temperatureConnected) && ok;
    ok = gResourcesManager.bindResource(&toggleLight) && ok;
    return ok;
}

// ---- loop-side synchronisation -----------------------------------------------------------------

/// @brief Writes a Managed value only when it differs, so each change publishes once.
/// Managed resources do not expose whether they hold a value yet, hence the caller's `initialised`.
template <typename Resource, typename T>
static void setIfChanged(Resource &resource, bool &initialised, const T &value)
{
    if (initialised && resource.getValue() == value)
        return;
    if (resource.setValue(value))
        initialised = true;
}

static void syncLight()
{
    static bool known = false;
    static bool lastMeasured = false;
    const bool measured = currentLightState();
    if (!known || measured != lastMeasured)
    {
        known = true;
        lastMeasured = measured;
        IoXpander.digitalWrite(BUTTON_1_LIGHT_PIN, !measured); // button LED is lit while the light is off
        sensorsReportPending = true;
    }

    if (lightHoldActive)
    {
        if (millis() - lightHoldStartMs < LIGHT_SETTLE_MS)
            return;
        lightHoldActive = false;
    }
    static bool lightInitialised = false;
    setIfChanged(light, lightInitialised, measured);
}

static void syncDoor()
{
    // A missing expander reads as 0xFF; publishing that would report a door that is not there.
    if (!IoXpander.isConnected())
        return;
    static bool doorInitialised = false;
    const bool open = isDoorOpen(IoXpander.getCurrentData());
    if (doorInitialised && door.getValue() == open)
        return;
    if (door.setValue(open))
        doorInitialised = true;
    sensorsReportPending = true;
}

static void syncTemperature()
{
    static bool connectedInitialised = false;
    static bool temperatureInitialised = false;
    const TempSensorStatus status = tempSensorStatus();
    setIfChanged(temperatureConnected, connectedInitialised, status.connected);
    // A lost sensor keeps its last retained reading; temperature_connected says it is no longer live.
    if (!isnan(status.tempC))
        setIfChanged(temperature, temperatureInitialised, status.tempC);
}

// ---- legacy reports ----------------------------------------------------------------------------
// The Dashboard still reads "<device>/sensors" and has not moved to the resource topics. Drop this
// once it has.

/// @brief {"light": bool, "door": bool, "ds18": float|null}, as the old firmware sent it.
/// Reads the cached expander sample rather than resampling: resampling fires the change callback on
/// whichever task asked, and this also runs for console commands on the MQTT task.
static String sensorsReport()
{
    DynamicJsonDocument doc(256);
    doc["light"] = currentLightState();
    doc["door"] = isDoorOpen(IoXpander.getCurrentData());
    // NAN while there is no sensor; ArduinoJson 6 serialises that as null.
    doc["ds18"] = currentTemperature();

    String json;
    serializeJson(doc, json);
    return json;
}

static String sensorsInfo()
{
    DynamicJsonDocument doc(2048);
    const TempSensorStatus ds18 = tempSensorStatus();
    JsonObject sensor = doc.createNestedObject("ds18");
    sensor["connected"] = ds18.connected;
    sensor["pin"] = DS18B20_PIN;
    sensor["address"] = ds18.address;
    sensor["parasite"] = ds18.parasite;
    sensor["value"] = ds18.tempC; // null while NAN
    sensor["type"] = "temperature";
    sensor["unit"] = "C";
    sensor["hardware"] = "DS18B20";
    JsonObject lightInfo = doc.createNestedObject("light");
    lightInfo["value"] = currentLightState();
    lightInfo["pin"] = LIGHT_RELAY_PIN;
    lightInfo["type"] = "boolean";
    lightInfo["hardware"] = "ZMPT101B";
    JsonObject doorInfo = doc.createNestedObject("door");
    doorInfo["value"] = isDoorOpen(IoXpander.getCurrentData());
    doorInfo["type"] = "boolean";
    doorInfo["pin"] = DOOR_SENSOR_PIN;
    doorInfo["hardware"] = "magnetic reed switch on PCF8574";

    String json;
    serializeJson(doc, json);
    return json;
}

static void publishSensorsReport()
{
    static uint32_t lastReportMs = 0;
    if (!sensorsReportPending && millis() - lastReportMs < SENSORS_REPORT_INTERVAL_MS)
        return;
    if (!MQTT_Connected())
        return; // stays pending until the broker is back
    if (MQTT_Publish("sensors", sensorsReport()))
    {
        sensorsReportPending = false;
        lastReportMs = millis();
    }
}

// ---- console -----------------------------------------------------------------------------------
// args[0] is the subcommand; argc counts it, so "IOXP WRITE 3 1" has argc == 3.

static NightMareResults handleIoxp(const NightMareMessage &message)
{
    NightMareResults res;
    res.result = false;
    if (message.subcommand == "READ")
    {
        res.result = true;
        if (message.argc < 2 || message.args[1] == "all")
            res.response = String(IoXpander.getCurrentData(true), HEX);
        else
            res.response = String(IoXpander.digitalRead(message.args[1].toInt()));
    }
    else if (message.subcommand == "WRITE")
    {
        if (message.argc < 3)
            res.response = "Not enough arguments for WRITE. Usage: IOXP WRITE <pin> <value>";
        else
        {
            bool value = message.args[2] == "1" || message.args[2] == "true";
            res.result = IoXpander.digitalWrite(message.args[1].toInt(), value);
            res.response = String(res.result);
        }
    }
    else if (message.subcommand == "SETINPUT")
    {
        if (message.argc < 2)
            res.response = "Not enough arguments for SETINPUT. Usage: IOXP SETINPUT <pin>";
        else
        {
            res.result = IoXpander.setAsInput(message.args[1].toInt());
            res.response = String(res.result);
        }
    }
    else if (message.subcommand == "REINIT")
    {
        // Each argument is optional and falls back to the board's wiring. Base 0 accepts "0x27".
        int sda = message.argc >= 2 ? message.args[1].toInt() : I2C_SDA_PIN;
        int scl = message.argc >= 3 ? message.args[2].toInt() : I2C_SCL_PIN;
        uint8_t address = message.argc >= 4 ? (uint8_t)strtol(message.args[3].c_str(), nullptr, 0)
                                            : IOXPANDER_DEFAULT_ADDRESS;
        res.result = IoXpander.begin(scl, sda, address);
        SystemState.setFlag("ioXpander_connected", res.result);
        res.response = res.result ? "IO Expander reinitialized successfully." : "Failed to reinitialize IO Expander.";
    }
    else if (message.subcommand == "TOGGLE")
    {
        if (message.argc < 2)
            res.response = "Not enough arguments for TOGGLE. Usage: IOXP TOGGLE <pin>";
        else
        {
            res.result = IoXpander.togglePin(message.args[1].toInt());
            res.response = String(res.result);
        }
    }
    else if (message.subcommand == "GETDATA")
    {
        res.result = true;
        res.response = String(IoXpander.getCurrentData(), HEX);
    }
    else if (message.subcommand == "SCAN")
    {
        res.result = true;
        res.response = IoXpander.scanBusJson();
    }
    else if (message.subcommand == "STATE")
    {
        res.result = true;
        res.response = IoXpander.getStateString();
    }
    else
        res.response = "Unknown IOXP subcommand available: [READ, WRITE, SETINPUT, REINIT, TOGGLE, GETDATA, SCAN, STATE].";
    return res;
}

static NightMareResults handleDs18(const NightMareMessage &message)
{
    NightMareResults res;
    res.result = false;
    // Both subcommands only read what the sampling task last published; neither touches the
    // 1-Wire bus, so they cannot collide with a conversion in progress.
    const TempSensorStatus ds18 = tempSensorStatus();
    if (message.subcommand == "READ")
    {
        res.result = !isnan(ds18.tempC);
        if (res.result)
            res.response = String(ds18.tempC, 2);
        else if (ds18.connected)
            res.response = "Sensor found, first conversion still running.";
        else
            res.response = "No DS18B20 found on GPIO" + String(DS18B20_PIN) + ".";
    }
    else if (message.subcommand == "STATUS")
    {
        DynamicJsonDocument doc(256);
        doc["connected"] = ds18.connected;
        doc["pin"] = DS18B20_PIN;
        doc["address"] = ds18.address;
        doc["parasite"] = ds18.parasite;
        doc["temperature"] = ds18.tempC; // null while NAN
        if (ds18.lastReadMs)
            doc["age_ms"] = millis() - ds18.lastReadMs;
        else
            doc["age_ms"] = nullptr; // never read
        serializeJson(doc, res.response);
        res.result = true;
    }
    else
        res.response = "Unknown DS18 subcommand available: [READ, STATUS].";
    return res;
}

static NightMareResults handleLight(const NightMareMessage &message)
{
    NightMareResults res;
    res.result = false;
    if (message.subcommand == "STATE")
    {
        res.result = true;
        res.response = String(currentLightState());
    }
    else if (message.subcommand == "TOGGLE")
    {
        res.result = IoXpander.togglePin(LIGHT_RELAY_PIN);
        res.response = res.result ? "Light toggled." : "IO expander did not respond.";
    }
    else if (message.subcommand == "SET")
    {
        // The Dashboard sends "light set toggle" here.
        const String &arg = message.args[1];
        bool isToggle = arg == "toggle" || arg == "t" || arg == "2";
        if (message.argc < 2)
            res.response = "Usage: LIGHT SET <1|0|toggle>";
        else
        {
            bool target = isToggle ? !currentLightState() : (arg == "1" || arg == "true");
            res.result = driveLight(target);
            // The measured state lags the relay, so report the target rather than a stale reading.
            res.response = res.result ? String(target) : "IO expander did not respond.";
        }
    }
    else
        res.response = "Unknown LIGHT subcommand available: [STATE, TOGGLE, SET].";
    return res;
}

static NightMareResults handleSensors(const NightMareMessage &message)
{
    NightMareResults res;
    res.result = true;
    if (message.subcommand == "REPORT")
        res.response = sensorsReport();
    else if (message.subcommand == "INFO")
        res.response = sensorsInfo();
    else
    {
        res.result = false;
        res.response = "Unknown SENSORS subcommand available: [REPORT, INFO].";
    }
    return res;
}

NightMareResults localHandleNightMareCommand(const NightMareMessage &message)
{
    NightMareResults res;
    if (message.command == "IOXP")
        res = handleIoxp(message);
    else if (message.command == "DS18")
        res = handleDs18(message);
    else if (message.command == "LIGHT")
        res = handleLight(message);
    else if (message.command == "SENSORS")
        res = handleSensors(message);
    else if (message.command == "HELP")
    {
        res.result = true;
        res.response = "Available commands: IOXP, DS18, LIGHT, SENSORS. Resources: >list";
    }
    else
    {
        res.result = false;
        res.response = "Unknown command available: [IOXP, DS18, LIGHT, SENSORS].";
    }
    LOG("Mycroft", "%s %s -> %s", message.command.c_str(), message.subcommand.c_str(), OK_LOG(res.result));
    return res;
}

// ---- hardware callbacks ------------------------------------------------------------------------

/// @brief Fired from IoXpander.update(). That normally runs in loop(), but a console command that
/// resamples the port runs it on the MQTT task -- so only hardware is touched here. The door is
/// picked up by syncDoor().
static void onIoXpanderChange(byte current, byte previous)
{
    byte diff = current ^ previous;
    if ((diff & (1 << BUTTON_1_PIN)) && (current & (1 << BUTTON_1_PIN)))
        IoXpander.togglePin(LIGHT_RELAY_PIN);
}

static void onMqttConnected()
{
    // Resources re-announce themselves; the legacy report does not.
    sensorsReportPending = true;
}

// ---- lifecycle ---------------------------------------------------------------------------------

void setup()
{
    introNightMareESP(); // starts Serial and prints the banner, name and firmware version

    setCommandResolver(localHandleNightMareCommand);
    if (!bindResources())
        LOG_ERROR("Mycroft", "one or more resources failed to bind");

    IoXpander.onChange(onIoXpanderChange);
    bool ioXpanderSuccess = IoXpander.begin();
    SystemState.setFlag("ioXpander_connected", ioXpanderSuccess);
    if (!ioXpanderSuccess)
        LOG_ERROR("Mycroft", "PCF8574 did not answer at 0x%02X", IOXPANDER_DEFAULT_ADDRESS);
    setupLightController();
    setupTempSensor();

    MQTT_onConnected(onMqttConnected);
    // WiFi_Auto() inside connects with the stored credentials; the first connection starts MQTT.
    startNightMareESP();
}

void loop()
{
    tickNightMareESP();

    static uint32_t lastPollMs = 0;
    if (millis() - lastPollMs < IOXPANDER_POLL_MS)
        return;
    lastPollMs = millis();

    IoXpander.update();
    syncLight();
    syncDoor();
    syncTemperature();
    publishSensorsReport();
}
