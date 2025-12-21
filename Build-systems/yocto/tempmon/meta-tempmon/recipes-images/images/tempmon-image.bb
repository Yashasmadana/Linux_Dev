SUMMARY = "Custom image with temperature monitor"
DESCRIPTION = "Minimal image including tempmon daemon"

LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL += " \
    tempmon \
    python3 \
    systemd \
"

IMAGE_FEATURES += "ssh-server-dropbear"

# Use systemd as init manager
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"

