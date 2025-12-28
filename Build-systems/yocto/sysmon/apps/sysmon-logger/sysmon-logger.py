import sqlite3
import time
import psutil
import os
from datetime import datetime

DB_PATH = "/tmp/sysmon.db"


def init_database():
    """Create database and table if not exists"""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS metrics (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp TEXT NOT NULL,
        cpu_percent REAL,
        memory_percent REAL,
        temp REAL,
        disk_percent REAL
    )
    """)
    conn.commit()
    conn.close()
    
def cleanup_old_data(conn):
    """Keep only last 24 hours of data"""
    cursor = conn.cursor()
    cursor.execute("""
    DELETE FROM metrics 
    WHERE timestamp < datetime('now', '-24 hours')
    """)
    conn.commit()
    
def collect_data():
    """Collect system metrics"""
    import psutil
    import random
    from datetime import datetime
    
    data = {
        'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'cpu_percent': psutil.cpu_percent(interval=1),
        'memory_percent': psutil.virtual_memory().percent,
        'temp': random.uniform(40.0, 75.0),  # Simulated
        'disk_percent': psutil.disk_usage('/').percent
    }
    return data

def log_data(conn, data):
    """Insert data into database"""
    cursor = conn.cursor()
    cursor.execute("""
    INSERT INTO metrics (timestamp, cpu_percent, memory_percent, temp, disk_percent)
    VALUES (?, ?, ?, ?, ?)
    """, (data['timestamp'], data['cpu_percent'], data['memory_percent'], 
          data['temp'], data['disk_percent']))
    conn.commit()
    
def main():
    init_database()
    conn = sqlite3.connect(DB_PATH)
    
    print("System Monitor Logger Started")
    print("=" * 50)
    
    while True:
        data = collect_data()
        print(f"{data['timestamp']} - CPU: {data['cpu_percent']:.1f}% | Memory: {data['memory_percent']:.1f}% | Temp: {data['temp']:.1f}°C | Disk: {data['disk_percent']:.1f}%")
        log_data(conn, data)
        cleanup_old_data(conn)
        time.sleep(10)

if __name__ == "__main__":
    main()
