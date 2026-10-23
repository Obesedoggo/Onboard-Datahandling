// Spacecraft OBC Platform on Arduino

#include <Arduino.h>
#include "Payload.h"

// Configuration
const long baud = 115200;
const long buf_size = 48;                  // maximum size for Telecommand
const long hkPeriod = 5000;                // period for house keeping report to ground station

// On-board time
uint32_t obtBase  = 0;                     // OBT value (in seconds) at the moment obtRefMs was recorded
uint32_t obtRefMs = 0;

// Scheduled image variables
bool imgPending = false;
uint32_t imgTime = 0;

// ---------- Platform State ----------
enum PfMode { MODE_SAFE, MODE_TARGET };
//bool plPower = false;

// ---------- Create payload object ----------
void sendPlEvent(const char* evt, const char* status);
Payload payload(sendPlEvent);

// ---------- Set initial PF state ----------
PfMode pfMode = MODE_SAFE;

int lastHkPeriod = 0;

// ---------- Time functions ----------

uint32_t getObt() {                                          // Returns current OBT in seconds
  return (obtBase + (millis() - obtRefMs) / 1000UL) % 86400UL; // base + elapsed seconds, wrapped at 24 h (86400 s)
}

void setObt(uint32_t secs) {               // Sets the on-board clock to the given time
  obtBase  = secs % 86400UL;               // Store the new time (wrapped into 0..86399 to be safe)
  obtRefMs = millis();                     // Remember the current arduino time so elapsed time is counted from here
}

bool parseTime(const char* s, uint32_t &outSecs) {           // Converts "hh:mm:ss" text to seconds; returns false if invalid
  int h, m, sec;                                             // Temporary variables for hours, minutes, seconds
  if (sscanf(s, "%d:%d:%d", &h, &m, &sec) != 3) return false; // Read three integers separated by ':'; fail if not exactly 3 found
  if (h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) return false; // Range check each part
  outSecs = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + (uint32_t)sec;           // Convert to total seconds since midnight
  return true;                                               // Parsing succeeded; result is in outSecs (passed by reference)
}

void printObt(uint32_t t) {                // Prints seconds-since-midnight as "hh:mm:ss" on the serial port
  char buf[9];                             // Text buffer: 8 characters "hh:mm:ss" + 1 for the '\0' terminator
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",             // Format with leading zeros ("%02lu" = 2-digit unsigned long)
           t / 3600UL, (t / 60UL) % 60UL, t % 60UL);          // hours, minutes, seconds computed from total seconds
  Serial.print(buf);                       // Send the formatted text (no newline; caller continues the line)
}

// ---------- Telemetry ----------

void sendAck(const char* seq, bool accepted) { // Sends TC verification for the command with sequence number seq
  Serial.print(F("TM_EVT_ACK,"));          // TM code
  printObt(getObt());                      // Field 1: OBT
  Serial.print(',');                       // Field separator
  Serial.print(seq);                       // Field 2: sequence number of the TC being acknowledged (echoed as text)
  Serial.print(',');                       // Field separator
  Serial.println(accepted ? F("ACCEPT") : F("REJECT")); // Field 3: verification result, ends the line
}

void sendModeEvent(PfMode oldM, PfMode newM) { // Reports a platform mode transition
  Serial.print(F("TM_EVT_MODE,"));         // TM code
  printObt(getObt());                      // Field 1: OBT
  Serial.print(',');                       // Field separator
  Serial.print(modeName(oldM));            // Field 2: previous mode
  Serial.print(',');                       // Field separator
  Serial.println(modeName(newM));          // Field 3: new mode, ends the line
}

void sendPlEvent(const char* evt, const char* status) { // Reports a payload event (power change, image, ...)
  Serial.print(F("TM_EVT_PL,"));           // TM code
  printObt(getObt());                      // Field 1: OBT
  Serial.print(',');                       // Field separator
  Serial.print(evt);                       // Field 2: event name, e.g. "IMAGE_TAKEN"
  Serial.print(',');                       // Field separator
  Serial.println(status);                  // Field 3: status text, ends the line
}

void sendHK() {
  Serial.print("TM_HK,Cyclic,");
  printObt(getObt());
  Serial.print(',');
  Serial.print(pfMode);
  Serial.print(',');
  Serial.print(payload.getPower());
  Serial.println();
}

// ---------- Platform and Payload Event ----------

void changeMode(PfMode newMode){
    PfMode oldMode = pfMode;
    if(strcmp(modeName(newMode), "SAFE") == 0){payload.setPower(false); } // turn off payload power when entering safe-mode
    delay(2000);
    pfMode = newMode;

    sendModeEvent(oldMode, newMode);
}

const char* modeName(PfMode m) { return (m == MODE_SAFE) ? "SAFE" : "TARGET"; } // Converts a mode enum to its text name

void scheduledImage(){
  payload.takeImage();
  imgPending = false;   // reset saved scheduled image command
  imgTime = 0;
}

// ---------- Telecommand handling ----------

void handleTC(char* line) {                // Parses and executes one complete command line (modified in place by strtok)
  // Split into tokens: code, seq, arg
  char* code = strtok(line, " ");          // First token: TC code, e.g. "TC_03"
  char* seq  = strtok(NULL, " ");          // Second token: sequence number (identifying number for this command)
  char* arg  = strtok(NULL, " ");          // Third token: argument (NULL if the TC has none)

  if (!code || !seq) return;               // Frame too short: no sequence number to ACK against, so ignore it

  bool tcAccepted = false;                         // Becomes true if the command is valid and accepted

  if (strcmp(code, "TC_01") == 0) {                       // TC_01 = SET_OBT
    uint32_t t;                                           // Will hold the parsed time in seconds
    if (arg && parseTime(arg, t)) {                       // Argument present and valid "hh:mm:ss"?
      tcAccepted = true;                                 // Mark as handled
      setObt(t);                                          // Then set the new on-board time
      sendAck(seq, true);
    }
  
  } else if (strcmp(code, "TC_02") == 0) {                  // TC_02 = SET_PF_MODE
    if (strcmp(modeName(pfMode), arg) != 0){
      if (arg && strcmp(arg, "SAFE") == 0) {                // Argument is "SAFE"
        tcAccepted = true;
        changeMode(MODE_SAFE);
        sendAck(seq, true);                                 // Acknowledge
      } else if (arg && strcmp(arg, "TARGET") == 0) {       // Argument is "TARGET"
        tcAccepted = true;
        changeMode(MODE_TARGET);
        sendAck(seq, true);                                 // Acknowledge
      }
    }
  } else if (strcmp(code, "TC_03") == 0) {                // TC_03 = SET_PL_POWER
    if (arg && (arg[0] == '0' || arg[0] == '1') && arg[1] == '\0') { // Argument is exactly "0" or "1"
      bool on = (arg[0] == '1');                          // Convert the argument to a boolean

      // Payload may only be powered in TARGET mode
      if (!(on && pfMode == MODE_SAFE) && on != payload.getPower()) {   // Reject only switch-on while in SAFE or if power state wouldnt change
      tcAccepted = true;                               // Mark as accepted
      payload.setPower(on);
      sendAck(seq, true);                              // send acknowledgement
      }
    }

  } else if (strcmp(code, "TC_04") == 0) {                  // TC_04 = TAKE_IMAGE
    uint32_t t;                                             // Will hold the parsed time tag in seconds
    if (arg && payload.getPower()) {                        // Needs an argument and a powered payload
      if (strcmp(arg, "0") == 0) {                          // "0" = take the image now
        tcAccepted = true;
        sendAck(seq, true);
        payload.takeImage();
        
      } else if (parseTime(arg, t) && t > getObt()) {       // Otherwise expect a time tag "hh:mm:ss"
        tcAccepted = true; 
        sendAck(seq, true);
        imgPending = true; 
        imgTime = t;                                        // Store the schedule (overwrites any earlier one)
        sendPlEvent("IMAGE_SCHEDULED", "OK");               // Report that the image is queued
      }
    }
  }
  // An unknown TC code falls through all branches with tcAccepted == false

  if (!tcAccepted) sendAck(seq, false);            // Anything not accepted above gets a REJECT
}

void readTC() {
  static char buf[buf_size];
  static int i_buf = 0;
  static bool overflow = false;            // Set when a line was too long and is being discarded

  while (Serial.available() > 0) {         // Process everything currently waiting in the serial input buffer
    char c = (char)Serial.read();          // Take one character

    if (c == '\n') {                       // Newline = end of the command line
      if (!overflow && i_buf > 0) {          // Only process if the line was not too long and not empty
        buf[i_buf] = '\0';                   // Terminate the text so it is a valid C string
        handleTC(buf);                     // Parse and execute the command
      }
      i_buf = 0; 
      overflow = false;           // Reset for the next line
    } else if (i_buf < buf_size - 1) {    // Room left (keeping one byte for '\0')?
      buf[i_buf++] = c;                      // Store the character and advance
    } else {
      overflow = true;                     // Buffer full: mark the line as invalid and drop the rest of it
      char overflow_msg[100];
      snprintf(overflow_msg, sizeof(overflow_msg), "TC was longer than expected. More than %d", buf_size);
      Serial.print("overflow_msg");
    }
  }
}

// ---------- Arduino Functions ----------
void setup() {
  Serial.begin(baud);

  lastHkPeriod = millis();
}

void loop() {
  readTC();

  // Housekeeping telemetry is send every hkPeriod seconds
  if (millis()-lastHkPeriod >= hkPeriod) {
    sendHK();
    lastHkPeriod = millis();
  }

  if (imgTime != 0 && getObt() >= imgTime) scheduledImage();
}
