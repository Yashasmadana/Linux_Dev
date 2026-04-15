#!/bin/bash
# ============================================================
# 02_preempt_rt/check_rt.sh
#
# TOPIC: Check if your kernel has PREEMPT_RT
#
# Run this on your BBB to check RT status
# Usage: chmod +x check_rt.sh && ./check_rt.sh
# ============================================================

echo ""
echo "╔══════════════════════════════════════╗"
echo "║   PREEMPT_RT Kernel Check            ║"
echo "╚══════════════════════════════════════╝"
echo ""

# 1. Kernel version
echo "=== Kernel Version ==="
uname -r
echo ""

# 2. Check for RT patch
echo "=== RT Preemption Type ==="
if [ -f /sys/kernel/realtime ]; then
    RT=$(cat /sys/kernel/realtime)
    if [ "$RT" = "1" ]; then
        echo "  ✅ PREEMPT_RT is ACTIVE"
    else
        echo "  ❌ Not a PREEMPT_RT kernel"
    fi
else
    echo "  ❌ /sys/kernel/realtime not found — not RT kernel"
fi

# Also check via kernel config
if [ -f /boot/config-$(uname -r) ]; then
    echo ""
    echo "=== Kernel Config (RT related) ==="
    grep -E "PREEMPT|PREEMPTION" /boot/config-$(uname -r) | head -10
fi

# 3. Check zcat /proc/config.gz
if [ -f /proc/config.gz ]; then
    echo ""
    echo "=== /proc/config.gz (RT related) ==="
    zcat /proc/config.gz | grep -E "PREEMPT|PREEMPTION" | head -10
fi

echo ""
echo "=== CPU Info ==="
grep "model name\|Hardware" /proc/cpuinfo | head -3

echo ""
echo "=== Current Scheduler ==="
cat /sys/kernel/debug/sched/features 2>/dev/null || echo "  (need root for sched features)"

echo ""
echo "=== Timer Resolution ==="
cat /proc/timer_list | grep "timer resolution" | head -3

echo ""
echo "================================================"
echo "If kernel is NOT PREEMPT_RT, follow these steps:"
echo ""
echo "  1. Get BBB kernel source:"
echo "     git clone https://github.com/beagleboard/linux.git"
echo "     cd linux && git checkout 5.10"
echo ""
echo "  2. Download matching PREEMPT_RT patch from:"
echo "     https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/"
echo ""
echo "  3. Apply patch:"
echo "     patch -p1 < patch-5.10.rt.patch"
echo ""
echo "  4. Configure:"
echo "     make ARCH=arm bb.org_defconfig"
echo "     make ARCH=arm menuconfig"
echo "     # Set: Kernel Features -> Preemption Model -> Fully Preemptible (RT)"
echo ""
echo "  5. Build & install:"
echo "     make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j4 zImage dtbs modules"
echo "     sudo make ARCH=arm modules_install"
echo "     sudo cp arch/arm/boot/zImage /boot/"
echo "     sudo reboot"
echo "================================================"
echo ""
