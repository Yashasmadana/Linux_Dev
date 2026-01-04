import sqlite3
import time
import psutil
from datetime import datetime

DB_PATH = "/tmp/sysmon.db"
INTERVAL = 10

def init_database():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS metrics (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        cpu_percent REAL,
        memory_percent REAL,
        temp REAL,
        disk_percent REAL
    )
    """)
    conn.commit()
    conn.close()


def cleanup_old_data(conn):
    cutoff = int(time.time()) - 24 * 3600
    conn.execute(
        "DELETE FROM metrics WHERE timestamp < ?",
        (cutoff,)
    )
    conn.commit()

def collect_data():
    temps = psutil.sensors_temperatures()
    temp = 0.0
    if temps:
        for k in temps:
            if temps[k]:
                temp = temps[k][0].current
                break

    return {
        "timestamp": int(time.time()),
        "cpu": psutil.cpu_percent(interval=1),
        "memory": psutil.virtual_memory().percent,
        "temp": temp,
        "disk": psutil.disk_usage("/").percent,
    }

def log_data(conn, data):
    conn.execute("""
        INSERT INTO metrics (timestamp, cpu_percent, memory_percent, temp, disk_percent)
        VALUES (?, ?, ?, ?, ?)
    """, (
        data["timestamp"],
        data["cpu"],
        data["memory"],
        data["temp"],
        data["disk"],
    ))
    conn.commit()

def main():
    init_database()
    conn = sqlite3.connect(DB_PATH, timeout=10)

    last_cleanup = 0
    print("SysMon Logger running...")

    while True:
        data = collect_data()
        print(
            f"{datetime.fromtimestamp(data['timestamp'])} | "
            f"CPU {data['cpu']:.1f}% | "
            f"MEM {data['memory']:.1f}% | "
            f"TEMP {data['temp']:.1f}°C | "
            f"DISK {data['disk']:.1f}%"
        )

        log_data(conn, data)

        if time.time() - last_cleanup > 3600:
            cleanup_old_data(conn)
            last_cleanup = time.time()

        time.sleep(INTERVAL)

if __name__ == "__main__":
    main()

