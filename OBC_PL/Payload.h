#include <Arduino.h>

typedef void (*EventCallback)(const char* evt, const char* status);

class Payload {
public:
  Payload(EventCallback cb);       // constructor: gets the event-reporting function
  bool setPower(bool on);               // returns true if the state changed
  bool getPower() const;
  void takeImage();

private:
  EventCallback report;
  bool powered;                         // Payload power state: false = OFF, true = ON
};