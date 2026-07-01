import serial
import time
import sys

try:
    ser = serial.Serial('/dev/cu.usbserial-5B1F0101911', 115200, timeout=1)
    print("Connected to serial port")
    end_time = time.time() + 10 # monitor for 10 seconds
    while time.time() < end_time:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', errors='replace').strip())
except Exception as e:
    print(f"Error: {e}")
