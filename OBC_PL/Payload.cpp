#include "Payload.h"

Payload::Payload(EventCallback cb) : report(cb), powered(false) {}

bool Payload::setPower(bool powerCommand) {
  if (powerCommand == powered) return false;         // returns falls if power already is true/false
  powered = powerCommand;
  report(powerCommand ? "PL_POWER_ON" : "PL_POWER_OFF", "OK");
  return true;
}

bool Payload::getPower() const { return powered; }
