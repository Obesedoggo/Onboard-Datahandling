#include <Arduino.h>

typedef void (*EventCallback)(const char* evt, const char* status); // Add the callback function to Pl class

class Payload {
public:                       // PL functions
  Payload(EventCallback cb);
  bool setPower(bool on);
  bool getPower() const;
  void takeImage();

private:                      // PL internal variable/function
  EventCallback report;
  bool powered;
};