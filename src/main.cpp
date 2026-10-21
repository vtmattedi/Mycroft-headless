#include <Arduino.h>
#include <NightMare.h>
#include <IoXpander.h>
#include <LightController.h>
#include <Door.h>
#include <TempSensor.h>

// Mycroft-headless: room light, door and temperature node.
//   light        ZMPT101B mains sense (the state) + relay on the PCF8574 (the actuator)
//   door         reed switch on the PCF8574
//   temperature  DS18B20
//
// Everything is a module with <x>Start() and, where it has work for loop(), <x>Tick(). Resources
// and Configs declare themselves inside their module and the framework binds them, so there is
// nothing to wire up here.

void setup()
{
    introNightMareESP(); // starts Serial and prints the banner, name and firmware version

    ioXpanderStart();
    lightStart();
    doorStart();
    tempStart();

    startNightMareESP();
}

void loop()
{
    tickNightMareESP();
    ioXpanderTick();
}
