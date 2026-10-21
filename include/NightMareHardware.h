#pragma once

#include <NightMare/HardwareProfile.h>

namespace NMHardware
{

// Physical topology only. Runtime capabilities belong in Resources and
// operator-tunable durable values belong in Configs. Mains entries document the
// installed wiring; they are not wiring instructions.
inline Profile projectProfile()
{
#ifdef ESP32_C3
    static constexpr const char *ControllerModel = "esp32-c3-supermini:v1";
    static constexpr const char *SdaGpio = "GPIO8";
    static constexpr const char *SclGpio = "GPIO9";
    static constexpr const char *MainsGpio = "GPIO3";
    static constexpr const char *TemperatureGpio = "GPIO7";
#else
    static constexpr const char *ControllerModel = "esp32-devkit-v1:v1";
    static constexpr const char *SdaGpio = "GPIO21";
    static constexpr const char *SclGpio = "GPIO22";
    static constexpr const char *MainsGpio = "GPIO32";
    static constexpr const char *TemperatureGpio = "GPIO4";
#endif

    static const Terminal controllerMcuTerminals[] = {
        {SdaGpio}, {SclGpio}, {MainsGpio}, {TemperatureGpio},
        {"3V3", CanonicalNet::V3v3}, {"5V", CanonicalNet::V5v},
        {"GND", CanonicalNet::Gnd},
    };
    static const Device controllerDevices[] = {
        {"mcu", controllerMcuTerminals, 7, "Controller MCU", "mcu", ControllerModel},
    };
    static const ConnectorContact controllerContacts[] = {
        {"5V", CanonicalNet::V5v}, {"GND_POWER", CanonicalNet::Gnd},
        {"3V3", CanonicalNet::V3v3}, {"GND_SIGNAL", CanonicalNet::Gnd},
        {"SDA"}, {"SCL"}, {"IO1"}, {"IO2"},
    };
    static const Connector controllerConnectors[] = {
        {"main", controllerContacts, 8, "Mycroft-X", ConnectorKind::Header},
    };
    static const Connection controllerConnections[] = {
        {{"", EndpointKind::DeviceTerminal, "mcu", "5V"},
         {"", EndpointKind::ConnectorContact, "main", "5V"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", "GND"},
         {"", EndpointKind::ConnectorContact, "main", "GND_POWER"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", "GND"},
         {"", EndpointKind::ConnectorContact, "main", "GND_SIGNAL"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", "3V3"},
         {"", EndpointKind::ConnectorContact, "main", "3V3"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", SdaGpio},
         {"", EndpointKind::ConnectorContact, "main", "SDA"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", SclGpio},
         {"", EndpointKind::ConnectorContact, "main", "SCL"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", TemperatureGpio},
         {"", EndpointKind::ConnectorContact, "main", "IO1"}},
        {{"", EndpointKind::DeviceTerminal, "mcu", MainsGpio},
         {"", EndpointKind::ConnectorContact, "main", "IO2"}},
    };

    static const Terminal temperatureTerminals[] = {
        {"VDD", CanonicalNet::V3v3}, {"DQ"}, {"GND", CanonicalNet::Gnd},
    };
    static const Terminal pullupTerminals[] = {{"1"}, {"2"}};
    static const Terminal zmptTerminals[] = {
        {"VCC", CanonicalNet::V3v3}, {"GND", CanonicalNet::Gnd},
        {"OUT"}, {"NEUTRAL", CanonicalNet::AcNeutral}, {"LIGHT_AC"},
    };
    static const Device mainDevices[] = {
        {"temperature", temperatureTerminals, 3, "Temperature sensor", "sensor", "DS18B20"},
        {"temperature_pullup", pullupTerminals, 2, "Temperature pull-up", "resistor", "4.7k ohm"},
        {"zmpt", zmptTerminals, 5, "Mains voltage sensor", "sensor", "ZMPT101B"},
    };
    static const ConnectorContact mainControllerContacts[] = {
        {"5V", CanonicalNet::V5v}, {"GND_POWER", CanonicalNet::Gnd},
        {"3V3", CanonicalNet::V3v3}, {"GND_SIGNAL", CanonicalNet::Gnd},
        {"SDA"}, {"SCL"}, {"IO1"}, {"IO2"},
    };
    static const ConnectorContact mainIoContacts[] = {
        {"5V", CanonicalNet::V5v}, {"GND_5V", CanonicalNet::Gnd},
        {"SDA"}, {"SCL"}, {"VCC", CanonicalNet::V3v3},
        {"GND_VCC", CanonicalNet::Gnd},
    };
    static const ConnectorContact mainMainsContacts[] = {
        {"AC_IN", CanonicalNet::AcPhase}, {"NEUTRAL_IN", CanonicalNet::AcNeutral},
    };
    static const ConnectorContact mainLightContacts[] = {
        {"LINE", CanonicalNet::AcPhase}, {"LIGHT_AC"},
    };
    static const Connector mainConnectors[] = {
        {"controller", mainControllerContacts, 8, "Controller", ConnectorKind::Header},
        {"io", mainIoContacts, 6, "Mycroft-Y-A", ConnectorKind::Header},
        {"mains", mainMainsContacts, 2, "Mains input", ConnectorKind::ScrewTerminal},
        {"light", mainLightContacts, 2, "Light circuit", ConnectorKind::ScrewTerminal},
    };
    static const Connection mainConnections[] = {
        {{"", EndpointKind::ConnectorContact, "controller", "5V"}, {"", EndpointKind::ConnectorContact, "io", "5V"}},
        {{"", EndpointKind::ConnectorContact, "controller", "GND_POWER"}, {"", EndpointKind::ConnectorContact, "controller", "GND_SIGNAL"}},
        {{"", EndpointKind::ConnectorContact, "controller", "GND_SIGNAL"}, {"", EndpointKind::ConnectorContact, "io", "GND_5V"}},
        {{"", EndpointKind::ConnectorContact, "io", "GND_5V"}, {"", EndpointKind::ConnectorContact, "io", "GND_VCC"}},
        {{"", EndpointKind::ConnectorContact, "io", "GND_VCC"}, {"", EndpointKind::DeviceTerminal, "zmpt", "GND"}},
        {{"", EndpointKind::ConnectorContact, "controller", "3V3"}, {"", EndpointKind::ConnectorContact, "io", "VCC"}},
        {{"", EndpointKind::ConnectorContact, "controller", "3V3"}, {"", EndpointKind::DeviceTerminal, "zmpt", "VCC"}},
        {{"", EndpointKind::ConnectorContact, "controller", "SDA"}, {"", EndpointKind::ConnectorContact, "io", "SDA"}},
        {{"", EndpointKind::ConnectorContact, "controller", "SCL"}, {"", EndpointKind::ConnectorContact, "io", "SCL"}},
        {{"", EndpointKind::ConnectorContact, "controller", "IO1"}, {"", EndpointKind::DeviceTerminal, "temperature", "DQ"}},
        {{"", EndpointKind::ConnectorContact, "controller", "3V3"}, {"", EndpointKind::DeviceTerminal, "temperature", "VDD"}},
        {{"", EndpointKind::ConnectorContact, "controller", "GND_SIGNAL"}, {"", EndpointKind::DeviceTerminal, "temperature", "GND"}},
        {{"", EndpointKind::DeviceTerminal, "temperature_pullup", "1"}, {"", EndpointKind::DeviceTerminal, "temperature", "DQ"}},
        {{"", EndpointKind::DeviceTerminal, "temperature_pullup", "2"}, {"", EndpointKind::DeviceTerminal, "temperature", "VDD"}},
        {{"", EndpointKind::ConnectorContact, "controller", "IO2"}, {"", EndpointKind::DeviceTerminal, "zmpt", "OUT"}},
        {{"", EndpointKind::ConnectorContact, "mains", "NEUTRAL_IN"}, {"", EndpointKind::DeviceTerminal, "zmpt", "NEUTRAL"}},
        {{"", EndpointKind::ConnectorContact, "light", "LIGHT_AC"}, {"", EndpointKind::DeviceTerminal, "zmpt", "LIGHT_AC"}},
        {{"", EndpointKind::ConnectorContact, "mains", "AC_IN"}, {"", EndpointKind::ConnectorContact, "light", "LINE"}},
    };

    static const Terminal expanderTerminals[] = {
        {"SDA"}, {"SCL"}, {"VCC", CanonicalNet::V3v3}, {"GND", CanonicalNet::Gnd},
        {"P0"}, {"P1"}, {"P3"}, {"P4"},
    };
    static const Device ioDevices[] = {
        {"expander", expanderTerminals, 8, "I/O expander", "io-expander", "PCF8574"},
    };
    static const ConnectorContact ioUpstreamContacts[] = {
        {"5V", CanonicalNet::V5v}, {"GND_5V", CanonicalNet::Gnd},
        {"SDA"}, {"SCL"}, {"VCC", CanonicalNet::V3v3}, {"GND_VCC", CanonicalNet::Gnd},
    };
    static const ConnectorContact signalContacts[] = {{"SIGNAL"}, {"GND", CanonicalNet::Gnd}};
    static const ConnectorContact relayControlContacts[] = {
        {"IN"}, {"VCC", CanonicalNet::V3v3}, {"GND", CanonicalNet::Gnd},
    };
    static const Connector ioConnectors[] = {
        {"upstream", ioUpstreamContacts, 6, "Mycroft-X", ConnectorKind::Header},
        {"p1", signalContacts, 2, "Button", ConnectorKind::Header},
        {"p2", signalContacts, 2, "Button LED", ConnectorKind::Header},
        {"p3", relayControlContacts, 3, "Relay", ConnectorKind::Header},
        {"p4", signalContacts, 2, "Door sensor", ConnectorKind::Header},
    };
    static const Connection ioConnections[] = {
        {{"", EndpointKind::ConnectorContact, "upstream", "SDA"}, {"", EndpointKind::DeviceTerminal, "expander", "SDA"}},
        {{"", EndpointKind::ConnectorContact, "upstream", "SCL"}, {"", EndpointKind::DeviceTerminal, "expander", "SCL"}},
        {{"", EndpointKind::ConnectorContact, "upstream", "VCC"}, {"", EndpointKind::DeviceTerminal, "expander", "VCC"}},
        {{"", EndpointKind::ConnectorContact, "upstream", "GND_VCC"}, {"", EndpointKind::DeviceTerminal, "expander", "GND"}},
        {{"", EndpointKind::ConnectorContact, "upstream", "GND_VCC"}, {"", EndpointKind::ConnectorContact, "p1", "GND"}},
        {{"", EndpointKind::ConnectorContact, "p1", "GND"}, {"", EndpointKind::ConnectorContact, "p2", "GND"}},
        {{"", EndpointKind::ConnectorContact, "p2", "GND"}, {"", EndpointKind::ConnectorContact, "p3", "GND"}},
        {{"", EndpointKind::ConnectorContact, "p3", "GND"}, {"", EndpointKind::ConnectorContact, "p4", "GND"}},
        {{"", EndpointKind::ConnectorContact, "upstream", "VCC"}, {"", EndpointKind::ConnectorContact, "p3", "VCC"}},
        {{"", EndpointKind::DeviceTerminal, "expander", "P0"}, {"", EndpointKind::ConnectorContact, "p1", "SIGNAL"}},
        {{"", EndpointKind::DeviceTerminal, "expander", "P1"}, {"", EndpointKind::ConnectorContact, "p2", "SIGNAL"}},
        {{"", EndpointKind::DeviceTerminal, "expander", "P3"}, {"", EndpointKind::ConnectorContact, "p3", "IN"}},
        {{"", EndpointKind::DeviceTerminal, "expander", "P4"}, {"", EndpointKind::ConnectorContact, "p4", "SIGNAL"}},
    };

    static const ConnectorContact relayContacts[] = {
        {"IN"}, {"VCC", CanonicalNet::V3v3}, {"GND", CanonicalNet::Gnd}, {"NO"}, {"C"}, {"NC"},
    };
    static const Connector relayConnectors[] = {{"terminals", relayContacts, 6, "Relay terminals", ConnectorKind::ScrewTerminal}};
    static const ConnectorContact buttonContacts[] = {{"SIGNAL"}, {"GND", CanonicalNet::Gnd}};
    static const Connector buttonConnectors[] = {{"lead", buttonContacts, 2, "Button lead", ConnectorKind::DirectWire}};
    static const ConnectorContact ledContacts[] = {{"A"}, {"K", CanonicalNet::Gnd}};
    static const Connector ledConnectors[] = {{"lead", ledContacts, 2, "LED lead", ConnectorKind::DirectWire}};
    static const ConnectorContact switchContacts[] = {{"NO"}, {"C"}, {"NC"}};
    static const Connector switchConnectors[] = {{"terminals", switchContacts, 3, "Wall switch", ConnectorKind::ScrewTerminal}};
    static const ConnectorContact lightContacts[] = {{"LINE", CanonicalNet::AcPhase}, {"LIGHT_AC_OUT"}};
    static const Connector lightConnectors[] = {{"terminals", lightContacts, 2, "Light", ConnectorKind::ScrewTerminal}};
    static const ConnectorContact mainsContacts[] = {{"AC", CanonicalNet::AcPhase}, {"NEUTRAL", CanonicalNet::AcNeutral}};
    static const Connector mainsConnectors[] = {{"terminals", mainsContacts, 2, "Mains supply", ConnectorKind::ScrewTerminal}};

    static const HardwareDefinition definitions[] = {
        {"controller-board", AssemblyKind::MarketBoard, "Controller", ControllerModel, nullptr, {nullptr, 0, controllerDevices, 1, controllerConnectors, 1, controllerConnections, 8}},
        {"mycroft-x", AssemblyKind::CustomBoard, "Mycroft-X", "mw-mycroft-x:v1.0", nullptr, {nullptr, 0, mainDevices, 3, mainConnectors, 4, mainConnections, 18}},
        {"mycroft-y-a", AssemblyKind::CustomBoard, "Mycroft-Y-A", "mw-mycroft-y-a:v1.0", nullptr, {nullptr, 0, ioDevices, 1, ioConnectors, 5, ioConnections, 13}},
        {"relay-module", AssemblyKind::Module, "SPDT relay module", nullptr, nullptr, {nullptr, 0, nullptr, 0, relayConnectors, 1}},
        {"button", AssemblyKind::Panel, "Momentary button", nullptr, nullptr, {nullptr, 0, nullptr, 0, buttonConnectors, 1}},
        {"button-led", AssemblyKind::Panel, "Button LED", nullptr, nullptr, {nullptr, 0, nullptr, 0, ledConnectors, 1}},
        {"door-sensor", AssemblyKind::External, "Door reed switch", nullptr, nullptr, {nullptr, 0, nullptr, 0, buttonConnectors, 1}},
        {"wall-switch", AssemblyKind::Panel, "SPDT wall switch", nullptr, nullptr, {nullptr, 0, nullptr, 0, switchConnectors, 1}},
        {"light", AssemblyKind::External, "AC light", nullptr, nullptr, {nullptr, 0, nullptr, 0, lightConnectors, 1}},
        {"mains", AssemblyKind::External, "AC mains input", nullptr, nullptr, {nullptr, 0, nullptr, 0, mainsConnectors, 1}},
    };
    static const Assembly roots[] = {
        {"controller", "controller-board", "Controller"}, {"main", "mycroft-x", "Main board"},
        {"io", "mycroft-y-a", "I/O board"}, {"relay", "relay-module", "Light relay"},
        {"button", "button", "Wall button"}, {"button_led", "button-led", "Button LED"},
        {"door", "door-sensor", "Door sensor"}, {"wall_switch", "wall-switch", "Wall switch"},
        {"light", "light", "Room light"}, {"mains", "mains", "Mains input"},
    };
    static const Connection connections[] = {
        {{"controller", EndpointKind::ConnectorContact, "main", "5V"}, {"main", EndpointKind::ConnectorContact, "controller", "5V"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "GND_POWER"}, {"main", EndpointKind::ConnectorContact, "controller", "GND_POWER"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "3V3"}, {"main", EndpointKind::ConnectorContact, "controller", "3V3"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "GND_SIGNAL"}, {"main", EndpointKind::ConnectorContact, "controller", "GND_SIGNAL"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "SDA"}, {"main", EndpointKind::ConnectorContact, "controller", "SDA"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "SCL"}, {"main", EndpointKind::ConnectorContact, "controller", "SCL"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "IO1"}, {"main", EndpointKind::ConnectorContact, "controller", "IO1"}},
        {{"controller", EndpointKind::ConnectorContact, "main", "IO2"}, {"main", EndpointKind::ConnectorContact, "controller", "IO2"}},
        {{"main", EndpointKind::ConnectorContact, "io", "5V"}, {"io", EndpointKind::ConnectorContact, "upstream", "5V"}},
        {{"main", EndpointKind::ConnectorContact, "io", "GND_5V"}, {"io", EndpointKind::ConnectorContact, "upstream", "GND_5V"}},
        {{"main", EndpointKind::ConnectorContact, "io", "SDA"}, {"io", EndpointKind::ConnectorContact, "upstream", "SDA"}},
        {{"main", EndpointKind::ConnectorContact, "io", "SCL"}, {"io", EndpointKind::ConnectorContact, "upstream", "SCL"}},
        {{"main", EndpointKind::ConnectorContact, "io", "VCC"}, {"io", EndpointKind::ConnectorContact, "upstream", "VCC"}},
        {{"main", EndpointKind::ConnectorContact, "io", "GND_VCC"}, {"io", EndpointKind::ConnectorContact, "upstream", "GND_VCC"}},
        {{"io", EndpointKind::ConnectorContact, "p1", "SIGNAL"}, {"button", EndpointKind::ConnectorContact, "lead", "SIGNAL"}},
        {{"io", EndpointKind::ConnectorContact, "p1", "GND"}, {"button", EndpointKind::ConnectorContact, "lead", "GND"}},
        {{"io", EndpointKind::ConnectorContact, "p2", "SIGNAL"}, {"button_led", EndpointKind::ConnectorContact, "lead", "A"}},
        {{"io", EndpointKind::ConnectorContact, "p2", "GND"}, {"button_led", EndpointKind::ConnectorContact, "lead", "K"}},
        {{"io", EndpointKind::ConnectorContact, "p4", "SIGNAL"}, {"door", EndpointKind::ConnectorContact, "lead", "SIGNAL"}},
        {{"io", EndpointKind::ConnectorContact, "p4", "GND"}, {"door", EndpointKind::ConnectorContact, "lead", "GND"}},
        {{"io", EndpointKind::ConnectorContact, "p3", "IN"}, {"relay", EndpointKind::ConnectorContact, "terminals", "IN"}},
        {{"io", EndpointKind::ConnectorContact, "p3", "VCC"}, {"relay", EndpointKind::ConnectorContact, "terminals", "VCC"}},
        {{"io", EndpointKind::ConnectorContact, "p3", "GND"}, {"relay", EndpointKind::ConnectorContact, "terminals", "GND"}},
        {{"mains", EndpointKind::ConnectorContact, "terminals", "AC"}, {"main", EndpointKind::ConnectorContact, "mains", "AC_IN"}},
        {{"mains", EndpointKind::ConnectorContact, "terminals", "NEUTRAL"}, {"main", EndpointKind::ConnectorContact, "mains", "NEUTRAL_IN"}},
        {{"main", EndpointKind::ConnectorContact, "light", "LINE"}, {"light", EndpointKind::ConnectorContact, "terminals", "LINE"}},
        {{"relay", EndpointKind::ConnectorContact, "terminals", "C"}, {"main", EndpointKind::ConnectorContact, "light", "LIGHT_AC"}},
        {{"relay", EndpointKind::ConnectorContact, "terminals", "NO"}, {"wall_switch", EndpointKind::ConnectorContact, "terminals", "NO"}},
        {{"relay", EndpointKind::ConnectorContact, "terminals", "NC"}, {"wall_switch", EndpointKind::ConnectorContact, "terminals", "NC"}},
        {{"wall_switch", EndpointKind::ConnectorContact, "terminals", "C"}, {"light", EndpointKind::ConnectorContact, "terminals", "LIGHT_AC_OUT"}},
    };

    return {"controller", definitions, sizeof(definitions) / sizeof(definitions[0]),
            roots, sizeof(roots) / sizeof(roots[0]), connections,
            sizeof(connections) / sizeof(connections[0])};
}

} // namespace NMHardware
