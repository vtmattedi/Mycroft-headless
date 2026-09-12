#include <Arduino.h>
#include "NightmareNetwork.h"
#include <ArduinoJson.h>
#include <Version.h>
#include <IoXpander.h>
#include <LightController.h>
#include <TempSensor.h>
void SendReport(int selector);
String SensorsReport();
String SensorsInfo();

enum SensorReportSelector
{
  SENSOR_REPORT = 0,
  SENSOR_INFO = 1
};

NightMareResults localHandleNightMareCommand(const NightMareMessage &message)
{
  NightMareResults res;
  res.result = false;
  res.response = "not implemented";

  if (message.command == "IOXP")
  {
    if (message.subcommand == "READ")
    {
      if (message.argc < 2 || message.args[1] == "all")
      {
        res.result = true;
        res.response = String(IoXpander.getCurrentData(true), HEX);
      }
      else
      {
        int pin = message.args[1].toInt();
        res.result = true;
        res.response = String(IoXpander.digitalRead(pin));
      }
    }
    else if (message.subcommand == "WRITE")
    {
      if (message.argc < 3)
      {
        res.result = false;
        res.response = "Not enough arguments for WRITE. Usage: IOXP WRITE <pin> <value>";
      }
      else
      {
        int pin = message.args[1].toInt();
        bool value = message.args[2] == "1" || message.args[2] == "true";
        res.result = IoXpander.digitalWrite(pin, value);
        res.response = String(res.result);
      }
    }
    else if (message.subcommand == "SETINPUT")
    {
      if (message.argc < 2)
      {
        res.result = false;
        res.response = "Not enough arguments for SETINPUT. Usage: IOXP SETINPUT <pin>";
      }
      else
      {
        int pin = message.args[1].toInt();
        res.result = IoXpander.setAsInput(pin);
        res.response = String(res.result);
      }
    }
    else if (message.subcommand == "REINIT")
    {
      int sda = message.args[1].toInt() | 22;                            // default to 22 if not provided
      int scl = message.args[2].toInt() | 21;                            // default to 21 if not provided
      int address = message.args[3].toInt() | IOXPANDER_DEFAULT_ADDRESS; // default to 0x20 if not provided
      bool success = IoXpander.begin(scl, sda, address);
      res.result = success;
      res.response = success ? "IO Expander reinitialized successfully." : "Failed to reinitialize IO Expander.";
    }
    else if (message.subcommand == "TOGGLE")
    {
      if (message.argc < 2)
      {
        res.result = false;
        res.response = "Not enough arguments for TOGGLE. Usage: IOXP TOGGLE <pin>";
      }
      else
      {
        int pin = message.args[1].toInt();
        res.result = IoXpander.togglePin(pin);
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
      res.response = IoXpander.scanBusJson();
      res.result = true;
    }
    else
    {
      res.result = false;
      res.response = "Unknown IOXP subcommand available: [READ, WRITE, SETINPUT, REINIT, SCAN].";
    }
  }
  else if (message.command == "DS18")
  {
    // Both subcommands only read what the sampling task last published; neither touches the
    // 1-Wire bus, so they cannot collide with a conversion in progress.
    TempSensorStatus ds18 = tempSensorStatus();
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
      auto doc = DynamicJsonDocument(256);
      doc["connected"] = ds18.connected;
      doc["pin"] = DS18B20_PIN;
      doc["address"] = ds18.address;
      doc["parasite"] = ds18.parasite;
      doc["temperature"] = ds18.tempC; // null while NAN
      if (ds18.lastReadMs)
        doc["age_ms"] = millis() - ds18.lastReadMs;
      else
        doc["age_ms"] = nullptr; // never read
      // Into a fresh String: serializeJson appends, and res.response already holds a default.
      String json;
      serializeJson(doc, json);
      res.response = json;
      res.result = true;
    }
    else
    {
      res.result = false;
      res.response = "Unknown DS18 subcommand available: [READ, STATUS].";
    }
  }
  else if (message.command == "LIGHT")
  {
    if (message.subcommand == "STATE")
    {
      res.result = true;
      res.response = String(currentLightState());
    }
    else if (message.subcommand == "TOGGLE")
    {
      IoXpander.togglePin(LIGHT_RELAY_PIN);
      res.result = true;
      res.response = "Light toggled.";
    }
    else if (message.subcommand == "SET")
    {
      bool new_state = message.args[1] == "1" || message.args[1] == "true";
      bool is_toggle = message.args[1] == "toggle" || message.args[1] == "t" || message.args[1] == "2";
      if (new_state != currentLightState() || is_toggle)
      {
        IoXpander.togglePin(LIGHT_RELAY_PIN);
      }
      res.result = true;
      res.response = String(currentLightState());
    }
    else
    {
      res.result = false;
      res.response = "Unknown LIGHT subcommand available: [STATE, UPDATE, TOGGLE, SET].";
    }
  }
  else if (message.command == "SENSORS")
  {
    if (message.subcommand == "REPORT")
    {
      res.result = true;
      res.response = SensorsReport();
    }
    else if (message.subcommand == "INFO")
    {
      res.result = true;
      res.response = SensorsInfo();
    }
    else
    {
      res.result = false;
      res.response = "Unknown SENSORS subcommand available: [REPORT, INFO].";
    }
  }
  else if (message.command == "HELP")
  {
    res.result = true;
    res.response = "Available commands: IOXP, DS18, LIGHT, SENSORS.";
  }
  else
  {
    res.result = false;
    res.response = "Unknown command available: [IOXP, DS18, LIGHT, SENSORS].";
  }
  String raw = message.command + " " + message.subcommand + " " + message.args[1] + " " + message.args[2] + " " + message.args[3] + " " + message.args[4];
  Serial.printf("input = %s, result = <%s>\n", raw.c_str(), OK_LOG(res.result));
  return res;
}

void onMQTTMessage(String topic, String message)
{
  // Topic is already treated: <MyID>/<subtopic> -> subtopic. If the topic is not for this device, ignore it.

  if (topic == "light")
  {
    bool newState = message == "1" || message == "true";
    bool is_toggle = message == "toggle" || message == "t" || message == "2";
    if (newState != currentLightState() || is_toggle)
    {
      IoXpander.togglePin(LIGHT_RELAY_PIN);
    }
  }
}

void onWifiConnected(bool firstConnection)
{
  Serial.println("WiFi Connected!");
  if (firstConnection)
  {
    MQTT_Init(REMOTE_MQTT);
    // autoSyncTime();
  }
}

void onIoXpanderChange(byte current, byte previous)
{
  byte diff = current ^ previous;
  // Serial.printf("IO Expander changed from 0x%02X to 0x%02X\n", previous, current);
  if (diff & (1 << BUTTON_1_PIN))
  {
    bool buttonState = (current & (1 << BUTTON_1_PIN)) != 0;
    // Serial.printf("Button 1 state changed to %s\n", buttonState ? "PRESSED" : "RELEASED");
    if (buttonState)
    {
      // IoXpander.digitalWrite(BUTTON_1_LIGHT_PIN, true);
      // Serial.println("Button 1 pressed: Light ON");
      IoXpander.togglePin(LIGHT_RELAY_PIN);
    }
    else
    {
      // IoXpander.digitalWrite(BUTTON_1_LIGHT_PIN, false);
      // Serial.println("Button 1 released: Light OFF");
    }
  }
  else if (diff & (1 << DOOR_SENSOR_PIN))
  {
    // bool doorState = (current & (1 << DOOR_SENSOR_PIN)) != 0;
    // Serial.printf("Door sensor state changed to %s\n", doorState ? "OPEN" : "CLOSED");
    SendReport(SENSOR_REPORT);
  }
}

void SendReport(int selector)
{
  String topic = "";
  String payload = "";
  if (selector == SENSOR_REPORT)
  {
    topic = "sensors";
    payload = SensorsReport();
  }
  else if (selector == SENSOR_INFO)
  {
    topic = "sensorsinfo";
    payload = SensorsInfo();
  }
  else
  {
    Serial.printf("Unknown report selector: %d\n", selector);
  }
  if (topic.length() > 0 && payload.length() > 0)
    MQTT_Send(topic, payload);
}

String SensorsInfo()
{
  auto doc = DynamicJsonDocument(2048);
  auto ds18 = tempSensorStatus();
  auto sensor = doc.createNestedObject("ds18");
  sensor["connected"] = ds18.connected;
  sensor["pin"] = DS18B20_PIN;
  sensor["address"] = ds18.address;
  sensor["parasite"] = ds18.parasite;
  sensor["value"] = ds18.tempC; // null while NAN
  sensor["type"] = "temperature";
  sensor["unit"] = "C";
  sensor["hardware"] = "DS18B20";
  auto light = doc.createNestedObject("light");
  light["value"] = currentLightState();
  light["pin"] = LIGHT_RELAY_PIN;
  light["type"] = "boolean";
  light["hardware"] = "ZMPT101B";
  auto door = doc.createNestedObject("door");
  door["value"] = (bool)((IoXpander.getCurrentData() >> DOOR_SENSOR_PIN) & 1);
  door["type"] = "boolean";
  door["pin"] = DOOR_SENSOR_PIN;
  door["hardware"] = "magnetic reed switch on PCF8574";

  String json;
  serializeJson(doc, json);
  return json;
}

String SensorsReport()
{
  auto doc = DynamicJsonDocument(512);

  // One snapshot of both bytes, so every field below describes the same instant.
  byte level = 0, latch = 0;
  IoXpander.snapshot(level, latch, true);

  doc["light"] = currentLightState();
  doc["door"] = (bool)((level >> DOOR_SENSOR_PIN) & 1);
  // NAN while there is no sensor; ArduinoJson 6 serialises that as null.
  doc["ds18"] = currentTemperature();

  String json;
  serializeJson(doc, json);
  return json;
}

void lightStateChanged(bool newState, float sensorValue)
{
  // Serial.printf("Light state changed to %s [%.2f]\n", newState ? "ON" : "OFF", sensorValue);
  IoXpander.digitalWrite(BUTTON_1_LIGHT_PIN, !newState);
  SendReport(SENSOR_REPORT);
}
void setup()
{
  Config.begin();
  Serial.begin(115200);
  Serial.println(DEVICE_NAME);
  Serial.printf("\tFirmware Version: %s\n", VERSION);
  Serial.printf("\tBuild Date: %s\n", BUILD_TIMESTAMP);
  Serial.println("Starting NightMare Network...");
  setCommandResolver(localHandleNightMareCommand);
  WiFi_onConnected(onWifiConnected);
  WiFi_Auto();
  // Serial.println(IoXpander.scanBus());
  IoXpander.onChange(onIoXpanderChange);
  bool ioXpanderSuccess = IoXpander.begin();
  Timers.create("ioXpander_update", 1, []()
                { IoXpander.update(); }, true);
  Timers.create("Sensors", 60, []()
                { SendReport(SENSOR_REPORT); }, false);
  SystemSettings.setFlag("ioXpander_connected", ioXpanderSuccess);
  MQTT_onMessage(onMQTTMessage, true);
  MQTT_onConnected([]()
                 { SendReport(SENSOR_INFO); });
  setupLightController();
  onLightStateChange(lightStateChanged);
  setupTempSensor();
}

void loop()
{
  Timers.run();
  scheduler.run();
  NightMareCommand_SerialResolver(&Serial, '\n');
}