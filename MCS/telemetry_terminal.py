import time
import os

def watch_telemetry(filename="mission_ops_log.txt"):
    print("=== LIVE TELEMETRY ===")
    print("Logs")
    
    if not os.path.exists(filename):
        with open(filename, "w") as f:
            f.write("=== LOG CREATED ===\n")

    with open(filename, "r") as file:
        file.seek(0, os.SEEK_END)
        
        while True:
            line = file.readline()
            if not line:
                time.sleep(0.5)
                continue

            print(line.strip())

watch_telemetry()