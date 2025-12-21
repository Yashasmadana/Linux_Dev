SUMMARY = "Temperature monitoring daemon"
DESCRIPTION = "Monitors system temperature and logs warnings"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://tempmon.py \
           file://tempmon.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} = "python3-core"

inherit systemd

SYSTEMD_SERVICE:${PN} = "tempmon.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/tempmon.py ${D}${bindir}/tempmon
    
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/tempmon.service ${D}${systemd_unitdir}/system
}

FILES:${PN} = "${bindir}/tempmon \
               ${systemd_unitdir}/system/tempmon.service \
              "
