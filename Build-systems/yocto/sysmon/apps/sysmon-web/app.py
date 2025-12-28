from flask import Flask, jsonify, render_template_string
import sqlite3
from datetime import datetime

DB_PATH = "/tmp/sysmon.db"

app = Flask(__name__)

def get_db():
    return sqlite3.connect(DB_PATH)

@app.route("/")
def index():
    return """
    <h2>SysMon Dashboard</h2>
    <p>Endpoints:</p>
    <ul>
        <li><a href="/api/latest">/api/latest</a></li>
        <li><a href="/api/history">/api/history</a></li>
    </ul>
    """

@app.route("/api/latest")
def latest():
    conn = get_db()
    cur = conn.cursor()

    cur.execute("""
        SELECT timestamp, cpu_percent, memory_percent, temp, disk_percent
        FROM metrics
        ORDER BY timestamp DESC
        LIMIT 1
    """)

    row = cur.fetchone()
    conn.close()

    if not row:
        return jsonify({})

    return jsonify({
        "timestamp": row[0],
        "time": datetime.fromtimestamp(row[0]).strftime("%Y-%m-%d %H:%M:%S"),
        "cpu_percent": row[1],
        "memory_percent": row[2],
        "temp": row[3],
        "disk_percent": row[4],
    })

@app.route("/api/history")
def history():
    conn = get_db()
    cur = conn.cursor()

    cur.execute("""
        SELECT timestamp, cpu_percent
        FROM metrics
        ORDER BY timestamp DESC
        LIMIT 60
    """)

    rows = cur.fetchall()
    conn.close()

    return jsonify([
        {
            "timestamp": r[0],
            "time": datetime.fromtimestamp(r[0]).strftime("%H:%M:%S"),
            "cpu_percent": r[1],
        } for r in reversed(rows)
    ])

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)
