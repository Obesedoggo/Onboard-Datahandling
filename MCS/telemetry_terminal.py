
import serial
import threading
import time

PORT = "COM3"
BAUD = 115200
LOGFILE = "mission_ops_log.txt"

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  #Delay to establish connection before commands 
ser.reset_input_buffer()

stop = threading.Event()

#Decode TM 
def format_tm(line):
    p = line.split()
    try:
        if p[0] == "TM_EVT_ACK":    #TM of Verification
            return f"[{p[1]}] ACK   TC #{p[2]} -> {p[3]}"
        if p[0] == "TM_EVT_MODE":   #TM of Mode transitions
            return f"[{p[1]}] MODE  {p[2]} -> {p[3]}"
        if p[0] == "TM_EVT_PL":     #TM of Payload events
            return f"[{p[1]}] PL    {p[2]} ({p[3]})"
        if p[0] == "TM_HK":         #TM 
            mode = "TARGET" if p[2] == "1" else "SAFE"
            power = "ON" if p[3] == "1" else "OFF"
            return f"[{p[1]}] HK    mode={mode} payload={power}"
    except IndexError:
        pass
    return f"[RAW] {line}"

#telemetry reader of TM from serialUSB port
def telemetry_reader():
    with open(LOGFILE, "a") as log:
        log.write("=== LIVE TELEMETRY SESSION ===\n")
        while not stop.is_set():
            raw = ser.readline().decode("utf-8", errors="ignore").strip()
            if not raw:
                continue
            text = format_tm(raw)
            print(text)
            log.write(text + "\n")
            log.flush()

#Telemetry window that prints TM from TM_reader 
def main():
    print("=== LIVE TELEMETRY ===")
    print("Type 'quit' to exit.\n")

    threading.Thread(target=telemetry_reader, daemon=True).start()

    seq = 1
    try: #Sends command from terminal to platform (Arduino)
        while True:
            cmd = input().strip()
            if cmd.lower() == "quit":
                break
            parts = cmd.split()
            if len(parts) != 2:
                print("Usage: <TC_code> <argument>")
                continue
            ser.write(f"{parts[0]} {seq} {parts[1]}\n".encode())
            seq += 1
    finally:
        stop.set()
        time.sleep(1.1)
        ser.close()


main() #will run TM_read in parallell with the sending of TC