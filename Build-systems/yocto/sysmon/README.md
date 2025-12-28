# System Monitor Challenge - Multi-Recipe Application Suite

## Overview
Complete system monitoring solution with data logger daemon, web dashboard, and SQLite database, all packaged as Yocto recipes.

## What I Built

### Components
1. **sysmon-logger** - Data collection daemon
   - Collects CPU, memory, temperature, disk usage
   - Stores data in SQLite database
   - Runs every 10 seconds as systemd service

2. **sysmon-web** - Web dashboard
   - Flask-based HTTP server
   - Displays real-time system metrics
   - Shows historical data with charts
   - Accessible at http://localhost:8080

3. **SQLite Database** - Data storage
   - Stores system metrics
   - Automatic cleanup (keeps 24 hours)

## Directory Structure
```
sysmon/
├── apps/                           # Development/testing applications
│   ├── sysmon-logger/
│   │   └── sysmon-logger.py
│   └── sysmon-web/
│       └── sysmon-web.py
├── meta-sysmon/                    # Yocto layer
│   ├── conf/
│   │   └── layer.conf
│   ├── recipes-sysmon/
│   │   ├── sysmon-logger/
│   │   │   ├── files/
│   │   │   │   ├── sysmon-logger.py
│   │   │   │   └── sysmon-logger.service
│   │   │   └── sysmon-logger_1.0.bb
│   │   └── sysmon-web/
│   │       ├── files/
│   │       │   ├── sysmon-web.py
│   │       │   └── sysmon-web.service
│   │       └── sysmon-web_1.0.bb
│   └── recipes-images/
│       └── images/
│           └── sysmon-image.bb
└── README.md
```

## How to Build

### Prerequisites
- Yocto/Poky installed
- 50GB+ free disk space

### Build Steps
```bash
# Set up environment
cd ~/poky
source oe-init-build-env

# Add the layer
bitbake-layers add-layer ~/Linux_Dev/Build-systems/yocto/sysmon/meta-sysmon

# Verify layer is added
bitbake-layers show-layers

# Build individual recipes (optional)
bitbake sysmon-logger
bitbake sysmon-web

# Build complete image
bitbake sysmon-image

# Run in QEMU
runqemu sysmon-image
```

### Testing in QEMU

Once booted:
```bash
# Check logger service
systemctl status sysmon-logger

# Check web service
systemctl status sysmon-web

# View logger logs
journalctl -u sysmon-logger -f

# View web logs
journalctl -u sysmon-web -f

# Check database
sqlite3 /tmp/sysmon.db "SELECT * FROM metrics LIMIT 10;"

# Access dashboard (from host browser)
http://192.168.7.2:8080
```

## Development Workflow

Applications were developed and tested natively on Ubuntu VM first:
```bash
# Test logger
cd apps/sysmon-logger
python3 sysmon-logger.py

# Test web dashboard
cd apps/sysmon-web
python3 sysmon-web.py
# Visit http://localhost:8080
```

Then packaged into Yocto recipes for embedded deployment.

## What I Learned

### Yocto Skills
- Creating custom layers with multiple recipes
- Managing recipe dependencies (RDEPENDS, DEPENDS)
- systemd service integration in recipes
- Custom image recipes
- Multi-component application packaging

### Embedded Linux Skills
- systemd service management
- Service dependencies and ordering
- Python daemon development
- SQLite in embedded systems
- Flask web applications

### System Programming
- psutil for system metrics collection
- SQLite database operations
- Web server implementation
- Data retention strategies

## Recipe Details

### sysmon-logger Recipe
- **RDEPENDS**: python3-core, python3-psutil, python3-sqlite3
- **Installs to**: /usr/bin/sysmon-logger
- **Service**: Auto-starts on boot, restarts on failure

### sysmon-web Recipe
- **RDEPENDS**: python3-core, python3-flask, python3-sqlite3
- **Installs to**: /usr/bin/sysmon-web
- **Service**: Starts after sysmon-logger, requires logger running

### sysmon-image Recipe
- Based on core-image
- Includes both applications and dependencies
- systemd as init system
- SSH access enabled

## Challenges Faced

1. **Multi-recipe dependencies** - Ensuring web service starts after logger
2. **Python packaging** - Understanding which Python modules to include
3. **Service ordering** - Proper systemd dependency configuration
4. **Database location** - Using /tmp for testing, would use /var/lib for production

## Time Taken
3 hours

## Future Improvements

- Add authentication to web dashboard
- Export data as CSV/JSON
- Email alerts on threshold violations
- Custom grafana-style charts
- Support for multiple database backends
- Container support (Docker/Podman)
- Real-time websocket updates instead of page refresh

## Technologies Used

- **Yocto Project** - Build system
- **BitBake** - Build engine
- **Python 3** - Application language
- **Flask** - Web framework
- **SQLite** - Database
- **systemd** - Service management
- **psutil** - System metrics
- **Git** - Version control
