// Spacecraft OBC Platform on Arduino


// Configuration
const long baud = 115200;
const long period = 10000;
const long buf_size = 48;                   // maximum size for command

// Platform(PF) State
enum PfMode { MODE_SAFE, MODE_TARGET };
bool PlPower = false;

// set initial PF state
PfMode = MODE_SAFE;

// ---------- Telecommand handling ----------
void handleTC(char* line) {                // Parses and executes one complete command line (modified in place by strtok)
  // Split into tokens: code, seq, arg
  char* code = strtok(line, " ");          // First token: TC code, e.g. "TC_03"
  char* seq  = strtok(NULL, " ");          // Second token: sequence number
  char* arg  = strtok(NULL, " ");          // Third token: argument (NULL if the TC has none)

  if (!code || !seq) return;               // Frame too short: no sequence number to ACK against, so ignore it

  bool ok = false;                         // Becomes true if the command is valid and accepted

  if (strcmp(code, "TC_01") == 0) {                       // TC_01 = SET_OBT
    uint32_t t;                                           // Will hold the parsed time in seconds
    if (arg && parseTime(arg, t)) {                       // Argument present and valid "hh:mm:ss"?
      ok = true;                                          // Mark as handled
      sendAck(seq, true);                                 // ACK first, so it carries the OBT from before the change
      setObt(t);                                          // Then set the new on-board time
    }

  } else if (strcmp(code, "TC_02") == 0) {                // TC_02 = SET_PF_MODE
    if (arg && strcmp(arg, "SAFE") == 0) {                // Argument is "SAFE"
      ok = true; sendAck(seq, true); changeMode(MODE_SAFE);     // ACK, then switch (this may emit further events)
    } else if (arg && strcmp(arg, "TARGET") == 0) {       // Argument is "TARGET"
      ok = true; sendAck(seq, true); changeMode(MODE_TARGET);   // ACK, then switch
    }

  } else if (strcmp(code, "TC_03") == 0) {                // TC_03 = SET_PL_POWER
    if (arg && (arg[0] == '0' || arg[0] == '1') && arg[1] == '\0') { // Argument is exactly "0" or "1"
      bool on = (arg[0] == '1');                          // Convert the character to a boolean
      // Example rule: payload may only be powered in TARGET mode
      if (!(on && pfMode == MODE_SAFE)) {                 // Reject only "switch ON while in SAFE"
        ok = true;                                        // Mark as handled
        sendAck(seq, true);                               // ACK first
        if (on != plPower) {                              // Only act if the state really changes
          plPower = on;                                   // Apply the new power state
          sendPlEvent(on ? "PL_POWER_ON" : "PL_POWER_OFF", "OK"); // Report the change
        }
      }
    }

  } else if (strcmp(code, "TC_04") == 0) {                // TC_04 = TAKE_IMAGE
    uint32_t t;                                           // Will hold the parsed time tag in seconds
    if (arg && plPower) {                                 // Needs an argument and a powered payload
      if (strcmp(arg, "0") == 0) {                        // "0" = take the image now
        ok = true; sendAck(seq, true); takeImage();       // ACK, then execute immediately
      } else if (parseTime(arg, t)) {                     // Otherwise expect a time tag "hh:mm:ss"
        ok = true; sendAck(seq, true);                    // ACK
        imgPending = true; imgTime = t;                   // Store the schedule (overwrites any earlier one)
        sendPlEvent("IMAGE_SCHEDULED", "OK");             // Report that the image is queued
      }
    }
  }
  // An unknown TC code falls through all branches with ok == false

  if (!ok) sendAck(seq, false);            // Anything not accepted above gets a REJECT
}

void readTC() {
  static char buf[buf_size]
  static int buf_cur_size = 0;

  while (Serial.available() > 0) {         // Process everything currently waiting in the serial input buffer
    char c = (char)Serial.read();          // Take one character

    //if (c == '\r') continue;               // Ignore carriage return (Windows-style line endings)
    if (c == '\n') {                       // Newline = end of the command line
      if (!overflow && len > 0) {          // Only process if the line was not too long and not empty
        buf[len] = '\0';                   // Terminate the text so it is a valid C string
        handleTC(buf);                     // Parse and execute the command
      }
      len = 0; overflow = false;           // Reset for the next line
    } else if (len < RX_BUF_SIZE - 1) {    // Room left (keeping one byte for '\0')?
      buf[len++] = c;                      // Store the character and advance

    } else {
      overflow = true;                     // Buffer full: mark the line as invalid and drop the rest of it
    }
  }
}

void setup() {


  serial.begin(baud);
  int x = 0;

}

void loop() {
  readTC();

}
