# Tempmon Challenge - Yocto Embedded Linux

## What I Built
Temperature monitoring daemon for embedded Linux using Yocto/BitBake.

## Components
- **Temperature monitor app** (`tempmon.py`) - Reads `/sys/class/thermal`, logs every 10s
- **systemd service** - Auto-starts daemon on boot
- **BitBake recipe** - Builds and packages the application
- **Custom image recipe** - Integrates tempmon into system image

## Structure
```
meta-tempmon/
├── recipes-tempmon/tempmon/
│   ├── files/
│   │   ├── tempmon.py
│   │   └── tempmon.service
│   └── tempmon_1.0.bb
└── recipes-images/images/
    └── tempmon-image.bb
```

## Build Commands
```bash
cd ~/poky && source oe-init-build-env
bitbake-layers add-layer /path/to/meta-tempmon
bitbake tempmon
bitbake tempmon-image
runqemu tempmon-image
```

## Skills Practiced
- Custom Yocto layers
- BitBake recipes and syntax
- systemd integration
- Embedded Linux workflows

## Challenge: 3 hours | Status: Completed (recipes validated, full build skipped due to disk space)
