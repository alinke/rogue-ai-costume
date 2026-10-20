#!/usr/bin/env python3
"""Set the costume's real-time clock to this computer's local time.

    python3 tools/set_clock.py [/dev/cu.wchusbserial110]
"""
import datetime
import glob
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else (glob.glob("/dev/cu.wchusbserial*") or [None])[0]
if not port:
    sys.exit("board not found: pass the serial port")
s = serial.Serial(port, 115200, timeout=0.5)
time.sleep(0.3)
s.reset_input_buffer()
now = datetime.datetime.now() + datetime.timedelta(seconds=1)
while datetime.datetime.now() < now.replace(microsecond=0):
    time.sleep(0.005)
s.write(("T" + now.strftime("%Y-%m-%d %H:%M:%S") + "\n").encode())
end = time.time() + 3
while time.time() < end:
    line = s.readline().decode("utf-8", "replace").strip()
    if "[clock]" in line:
        print(line)
        break
