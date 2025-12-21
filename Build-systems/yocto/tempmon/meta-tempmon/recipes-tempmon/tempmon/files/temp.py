#!/usr/bin/env python3
import time
import os

TEMP_FILE = "/sys/class/thermal/thermal_zone0/temp"
THRESHOLD = 80000

while True:
    # Your code here: read temp, log it, check threshold
    time.sleep(10)

