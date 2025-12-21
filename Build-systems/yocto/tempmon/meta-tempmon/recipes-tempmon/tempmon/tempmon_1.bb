SUMMARY = "Temperature monitoring daemon"
DESCRIPTION = "Monitors system temperature and logs warnings"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://tempmon.py"

S = "${WORKDIR}"

RDEPENDS:${PN} = "python3-core"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/tempmon.py ${D}${bindir}/tempmon
}

FILES:${PN} = "${bindir}/tempmon"
