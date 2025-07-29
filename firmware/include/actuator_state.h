#pragma once

struct ActuatorState {
  bool fan = false;
  bool pump = false;
  bool air = false;
  bool light = false;
  bool valve = false;
};

extern ActuatorState actuators;
