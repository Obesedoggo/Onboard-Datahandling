#include "Payload.h"

Payload::Payload(EventCallback cb) : report(cb), powered(false) {}    // Callback function for Pl subsystem to report events PL->PF->Gnd

bool Payload::setPower(bool powerCommand) {
  if (powerCommand == powered) return false;         // returns false if power already is true/false
  powered = powerCommand;
  report(powerCommand ? "PL_POWER_ON" : "PL_POWER_OFF", "OK");  // report event
  return true;
}

bool Payload::getPower() const { return powered; }

void Payload::takeImage(){
  report("TAKE_IMAGE", "START");      // report start
  delay(5000);                        // simulate required time to generate data
  report("TAKE_IMAGE", "FINSIHED");   // report finish
  return;
}
