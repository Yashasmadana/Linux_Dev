SUMMARY = "System monitor dashboard web server"
DESCRIPTION = "Web interface displaying system monitoring data from sysmon-logger"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://sysmon-web.py \
           file://sysmon-web.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} = "python3-core python3-flask python3-sqlite3"

inherit systemd

SYSTEMD_SERVICE:${PN} = "sysmon-web.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    # Install the Python script to /usr/bin
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/sysmon-web.py ${D}${bindir}/sysmon-web
    
    # Install the systemd service file
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/sysmon-web.service ${D}${systemd_unitdir}/system
}

FILES:${PN} = "${bindir}/sysmon-web \
               ${systemd_unitdir}/system/sysmon-web.service \
              "
