#!/usr/bin/env python3
import time
import os
import random

TEMP_FILE = "/sys/class/thermal/thermal_zone0/temp"
THRESHOLD = 80000  # 80°C in millidegrees

def read_temperature():
    """Read temperature from system or simulate if not available"""
    try:
        if os.path.exists(TEMP_FILE):
            with open(TEMP_FILE, 'r') as f:
                temp = int(f.read().strip())
                return temp / 1000.0  # Convert millidegrees to degrees
        else:
            # Simulate temperature between 40-85°C
            return random.uniform(40.0, 85.0)
    except Exception as e:
        print(f"Error reading temperature: {e}")
        return None

def main():
    print("Temperature Monitor Daemon Started")
    print("=" * 50)
    
    while True:
        temp = read_temperature()
        
        if temp is not None:
            temp_millidegrees = temp * 1000
            timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
            
            if temp_millidegrees > THRESHOLD:
                print(f"{timestamp} - WARNING: Temperature {temp:.1f}°C exceeds threshold!")
            else:
                print(f"{timestamp} - Temperature: {temp:.1f}°C")
        
        time.sleep(10)

if __name__ == "__main__":
    main()
