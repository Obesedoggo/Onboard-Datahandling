import time

class spacecraftplatform:
    def __init__(self):
        self.mode = "Safe-Mode"
        self.payload_power = False
        self.obt = 0

    def set_pf_mode(self, arg):
        #send to obs, obs back
        if arg == self.mode:
            print(f"Spacecraftplatorm already in {self.mode}")
        else:
            time.sleep(1)
            self.mode = arg
            print(f"Mode set as {arg}")

    def set_pl_power(self, arg):
        #send to obs, obs back
        if self.mode == "Safe-Mode":
            self.payload_power = False
        if arg == "1":
            self.payload_power = True
        elif arg == "0":
            self.payload_power = False

    def take_image(self):
        #send to obs, obs back
        if self.payload_power == True:
            time_take = getattr(self, "obt_offset",0)
            time.sleep(time_take)
            print("Image taken")

    def get_obt(self):
        #send to obs, obs back
        total_seconds = (time.localtime().tm_hour * 3600 + time.localtime().tm_min * 60 + time.localtime().tm_sec) % 86400
        h = total_seconds // 3600
        m = (total_seconds % 3600)//60
        s = total_seconds % 60
        return f"{h:02d}:{m:02d}:{s:02d}"

    def set_obt(self, arg):
        #send to obs, obs back
        h,m,s = map(int, arg.split(":"))
        target_time = h*3600 + m*60 + s
        current_time = time.localtime().tm_hour * 3600 + time.localtime().tm_min * 60 + time.localtime().tm_sec
        self.obt_offset = target_time - current_time
        self.obt = target_time
        print(f"OBT set to {arg}")

#append messages with time in file
    def log_event(log_type, message):
        h = time.localtime().tm_hour
        m = time.localtime().tm_min
        s = time.localtime().tm_sec
        timestamp = (f"{h}:{m}:{s}")
        log_entry = f"[{timestamp}] [{log_type}] {message}\n"
        with open("mission_ops_log.txt", "a") as file:
            file.write(log_entry)
        print(f"Sent -> {message}")

    def operator_console(self):
        print("=== MCS OPERATOR COMMAND TERMINAL ===")
        print("Type commands (example: SET-OBT 12:00:00 or exit):\n")
        while True:
            user_input = input("MCS-CMD> ").strip()
            if user_input.lower() == "exit":
                break
            part = user_input.split(" ")
            command = part[0]
            arg = part[1]
            alt_command = command.replace("-","_")
            MIB_valid = False
            with open ("MIB.txt", "r") as file:
                for line in file:
                    if alt_command in line:
                        MIB_valid = True
                        break
            if MIB_valid:
                if command == "SET-OBT":
                    self.set_obt(arg)
                    self.log_event("TC SENT" f"set obt {arg}")
                elif command == "SET-PF-MODE":
                    self.set_pf_mode(arg)
                    self.log_event("TC SENT" f"set pf mode {arg}")
                elif command == "TAKE-IMAGE":
                    self.take_image()
                    self.log_event("TC SENT" f"image taken")
                elif command == "SET-PL-POWER":
                    self.set_pl_power(arg)
                    self.log_event("TC SENT" f"set pl power {arg}")
            else:
                print ("Function or value not in MIB")

s = spacecraftplatform()
s.operator_console()