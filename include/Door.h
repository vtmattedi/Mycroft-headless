#pragma once
#include <Arduino.h>

/// Door reed switch on the IO expander. Resource: door (read-only, true = open).

/// @brief Publishes the initial state. Call after ioXpanderStart().
void doorStart();
/// @brief Feeds an expander change to the door; publishes only when the state actually moved.
void doorOnChange(byte current);
