SUMMARY = "System monitor data logger daemon"
DESCRIPTION = "Collects CPU, memory, temperature, and disk usage data"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://sysmon-logger.py \
           file://sysmon-logger.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} = "python3-core python3-psutil python3-sqlite3"

inherit systemd

SYSTEMD_SERVICE:${PN} = "sysmon-logger.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    # Install the Python script to /usr/bin
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/sysmon-logger.py ${D}${bindir}/sysmon-logger
    
    # Install the systemd service file
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/sysmon-logger.service ${D}${systemd_unitdir}/system
}

FILES:${PN} = "${bindir}/sysmon-logger \
               ${systemd_unitdir}/system/sysmon-logger.service \
              "
