#pragma once

// Initializes the float switch pin
void float_switch_init();
// Returns true if float switch is currently LOW (water is low)
bool float_switch_active();
// Returns true **only once** when float transitions from HIGH → LOW (water just became low)
bool is_float_switch_triggered();
