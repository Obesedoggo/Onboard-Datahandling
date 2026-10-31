import os
import subprocess
import sys
import threading
import time
import serial

PORT = "COM3"
BAUD = 115200
LOGFILE = "mission_ops_log.txt"
MIB_FILE = "MIB.txt"

log_lock = threading.Lock()

#Opens missions ops log as seperate window where TM and TC is saved
def watch_telemetry(filename=LOGFILE):
    print("=== LIVE TELEMETRY ===")
    if not os.path.exists(filename):
        with open(filename, "w") as f:
            f.write("=== LOG CREATED ===\n")

    with open(filename, "r") as file:
        file.seek(0, os.SEEK_END)  # only show new lines
        while True:
            line = file.readline()
            if not line:
                time.sleep(0.2)
                continue
            print(line.strip())


#S/C PF class
class spacecraftplatform:
    COMMANDS = {
        "SET-OBT":      ("TC_01", True),
        "SET-PF-MODE":  ("TC_02", True),
        "SET-PL-POWER": ("TC_03", True),
        "TAKE-IMAGE":   ("TC_04", False),
    }

    def __init__(self):
        self.mode = "SAFE"          # updated from telemetry
        self.payload_power = False  # updated from telemetry
        self.seq = 1
        self.stop = threading.Event()

        self.ser = serial.Serial(PORT, BAUD, timeout=1)
        time.sleep(2)  # Arduino resets when the port opens
        self.ser.reset_input_buffer()

        self.log_event("SYSTEM", "Operator console started")
        threading.Thread(target=self.telemetry_reader, daemon=True).start()

    #Saves events in mission ops
    def log_event(self, log_type, message):
        timestamp = time.strftime("%H:%M:%S")
        entry = f"[{timestamp}] [{log_type}] {message}\n"
        with log_lock:
            with open(LOGFILE, "a") as f:
                f.write(entry)

    #TM window 
    def telemetry_reader(self):
        while not self.stop.is_set():
            try:
                raw = self.ser.readline().decode("utf-8", errors="ignore").strip()
            except serial.SerialException:
                self.log_event("ERROR", "Serial connection lost")
                return
            if not raw:
                continue

            p = raw.split()
            try:
                if p[0] == "TM_EVT_ACK":      # TM_EVT_ACK <obt> <seq> ACCEPT|REJECT
                    self.log_event("TM ACK", f"OBT {p[1]} | TC #{p[2]} {p[3]}")
                elif p[0] == "TM_EVT_MODE":   # TM_EVT_MODE <obt> <old> <new>
                    self.mode = p[3]
                    self.log_event("TM MODE", f"OBT {p[1]} | {p[2]} -> {p[3]}")
                elif p[0] == "TM_EVT_PL":     # TM_EVT_PL <obt> <event> <status>
                    self.log_event("TM PL", f"OBT {p[1]} | {p[2]} ({p[3]})")
                elif p[0] == "TM_HK":         # TM_HK <obt> <mode> <power>
                    self.mode = "TARGET" if p[2] in ("1", "TARGET") else "SAFE"
                    self.payload_power = (p[3] == "1")
                    self.log_event(
                        "TM HK",
                        f"OBT {p[1]} | mode={self.mode} "
                        f"payload={'ON' if self.payload_power else 'OFF'}")
                else:
                    self.log_event("TM RAW", raw)
            except IndexError:
                self.log_event("TM RAW", raw)

    #sends TC to arduino
    def send_tc(self, tc_code, arg):
        line = f"{tc_code} {self.seq} {arg}\n"
        self.ser.write(line.encode())
        self.ser.flush()
        seq = self.seq
        self.seq += 1
        return seq

    #Following functions define format of TC to be sent to arduino
    def set_obt(self, arg):
        seq = self.send_tc("TC_01", arg)
        self.log_event("TC SENT", f"#{seq} set obt {arg}")

    def set_pf_mode(self, arg):
        seq = self.send_tc("TC_02", arg)
        self.log_event("TC SENT", f"#{seq} set pf mode {arg}")

    def set_pl_power(self, arg):
        seq = self.send_tc("TC_03", arg)
        self.log_event("TC SENT", f"#{seq} set pl power {arg}")

    def take_image(self, arg="0"):
        seq = self.send_tc("TC_04", arg)
        self.log_event("TC SENT", f"#{seq} take image {arg}")

    #commands sent from operator console are verified in MIB.txt file before being sent
    def in_mib(self, command):
        alt_command = command.replace("-", "_")
        try:
            with open(MIB_FILE, "r") as file:
                return any(alt_command in line for line in file)
        except FileNotFoundError:
            print(f"{MIB_FILE} not found")
            return False

    #Operator console setup
    def operator_console(self):
        print("=== MCS OPERATOR COMMAND TERMINAL ===")
        print("Commands:")
        print("  SET-OBT hh:mm:ss")
        print("  SET-PF-MODE SAFE|TARGET")
        print("  SET-PL-POWER 0|1")
        print("  TAKE-IMAGE [0|hh:mm:ss]    (0 or no argument = now)")
        print("Type 'exit' to quit.\n")

        while True:
            user_input = input("MCS-CMD> ").strip()
            if not user_input:
                continue
            if user_input.lower() == "exit":
                break
            #split commands into command, argument
            part = user_input.split()
            command = part[0].upper()
            arg = part[1] if len(part) > 1 else None

            #check for command in commands (defined in class) or MIB
            if command not in self.COMMANDS or not self.in_mib(command):
                print("Function or value not in MIB")
                self.log_event("MCS", f"Rejected locally: {user_input}")
                continue

            needs_arg = self.COMMANDS[command][1]
            if needs_arg and arg is None:
                print(f"{command} needs an argument")
                continue

            if command == "SET-OBT":
                self.set_obt(arg)
            elif command == "SET-PF-MODE":
                self.set_pf_mode(arg.upper())
            elif command == "SET-PL-POWER":
                self.set_pl_power(arg)
            elif command == "TAKE-IMAGE":
                self.take_image(arg or "0")
            print(f"Sent -> {user_input}")

    def close(self):
        self.stop.set()
        time.sleep(1.1)
        self.ser.close()


#Open mission ops log
def open_telemetry_window():
    if not os.path.exists(LOGFILE):
        with open(LOGFILE, "w") as f:
            f.write("=== LOG CREATED ===\n")
    cmd = [sys.executable, os.path.abspath(__file__), "--telemetry"]
    if os.name == "nt":
        subprocess.Popen(cmd, creationflags=subprocess.CREATE_NEW_CONSOLE)
    else:
        print("Open a second terminal and run:  python "
              f"{os.path.basename(__file__)} --telemetry")

#If "--telemetry" is in mission ops, watch telemetry will run and read this log 
if __name__ == "__main__":
    if "--telemetry" in sys.argv:
        watch_telemetry()
    #"--telemetry" isnt in log, initialize log to start script
    else:
        open_telemetry_window()
        s = spacecraftplatform()
        try:
            s.operator_console()
        finally:
            s.close()