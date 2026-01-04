#!/usr/bin/env python3
from flask import Flask, jsonify, render_template
import sqlite3

DB_PATH = "/tmp/sysmon.db"

app = Flask(__name__)

def get_db():
    return sqlite3.connect(DB_PATH)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/history")
def history():
    conn = get_db()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT timestamp, cpu_percent
        FROM metrics
        WHERE timestamp >= datetime('now', '-10 minutes')
        ORDER BY timestamp ASC
    """)

    rows = cursor.fetchall()
    conn.close()

    data = [
        {"timestamp": row[0], "cpu": row[1]}
        for row in rows
    ]

    return jsonify(data)

@app.route("/api/latest")
def latest():
    conn = get_db()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT timestamp, cpu_percent, memory_percent, temp, disk_percent
        FROM metrics
        ORDER BY timestamp DESC
        LIMIT 1
    """)

    row = cursor.fetchone()
    conn.close()

    if row is None:
        return jsonify({"error": "No data"}), 404

    return jsonify({
        "timestamp": row[0],
        "cpu_percent": row[1],
        "memory_percent": row[2],
        "temp": row[3],
        "disk_percent": row[4]
    })

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)

