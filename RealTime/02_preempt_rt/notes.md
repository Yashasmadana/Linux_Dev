# 02 — PREEMPT_RT

## What is PREEMPT_RT?

Standard Linux kernel cannot guarantee when a task will run.
PREEMPT_RT patches the kernel so **every task gets a guaranteed response time**.

```
Standard Kernel:        PREEMPT_RT Kernel:
─────────────────       ──────────────────
IRQ handler runs        IRQ handler becomes a thread
  non-preemptible         → can be preempted by RT task

Spinlocks block         Spinlocks converted to mutexes
  everything              → RT task can preempt

Worst case latency:     Worst case latency:
  ~100ms                  ~50 microseconds
```

## Key Concepts

### Preemption Models (menuconfig options)
```
1. No Forced Preemption       — server workloads, max throughput
2. Voluntary Preemption       — desktop, some latency
3. Preemptible Kernel         — better latency (~1ms)
4. Fully Preemptible (RT)     — PREEMPT_RT, ~50μs latency  ← we want this
```

### What PREEMPT_RT actually changes
```
- Converts hardirq/softirq to threaded interrupts
- Converts spinlocks to rt_mutexes (support priority inheritance)
- Makes critical sections preemptible
- Adds priority inheritance to avoid priority inversion
```

## Steps for BBB

### Step 1 — Check current kernel
```bash
./check_rt.sh
```

### Step 2 — Build RT kernel (if needed)
```bash
# On your Linux VM
git clone https://github.com/beagleboard/linux.git -b 5.10 --depth=1
cd linux

# Download RT patch (match your kernel version)
wget https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/5.10/patch-5.10.rt.patch.gz
zcat patch-5.10.rt.patch.gz | patch -p1

# Configure for BBB
make ARCH=arm bb.org_defconfig
make ARCH=arm menuconfig
# Navigate to: Kernel Features -> Preemption Model -> Fully Preemptible Kernel (RT)

# Build (takes 30-60 min)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs modules

# Install on BBB
scp arch/arm/boot/zImage debian@192.168.7.2:/tmp/
ssh debian@192.168.7.2 "sudo cp /tmp/zImage /boot/vmlinuz-rt && sudo reboot"
```

### Step 3 — Verify after reboot
```bash
./check_rt.sh
uname -r  # should show PREEMPT_RT in version string
```

## Next Step
→ Go to `03_latency/` to measure how much RT improved your response time
