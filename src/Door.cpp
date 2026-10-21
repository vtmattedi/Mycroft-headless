#include "Door.h"
#include <NightMare.h>
#include <IoXpander.h>

static ManagedSensor<bool> door("door");

static void publish(byte level)
{
    // A missing expander reads as 0xFF; publishing that would report a door that is not there.
    if (!IoXpander.isConnected())
        return;
    const bool open = (level >> DOOR_SENSOR_PIN) & 1;
    static bool published = false;
    if (published && door.getValue() == open)
        return;
    published = door.setValue(open);
}

void doorStart()
{
    publish(IoXpander.getCurrentData());
}

void doorOnChange(byte current)
{
    publish(current);
}
