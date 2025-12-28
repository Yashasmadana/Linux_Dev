SUMMARY = "Custom image with system monitoring suite"
DESCRIPTION = "Image including data logger, web dashboard, and dependencies"

LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL += " \
    sysmon-logger \
    sysmon-web \
    python3 \
    python3-psutil \
    python3-flask \
    python3-sqlite3 \
    sqlite3 \
    systemd \
"

IMAGE_FEATURES += "ssh-server-dropbear"

# Use systemd as init manager
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"
