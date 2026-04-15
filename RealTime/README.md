# Real-Time Linux on BeagleBone Black

Learning path for Real-Time Linux internals using BBB as the target platform.

## Structure

```
RealTime/
├── 01_scheduling/   → Linux scheduler: CFS, nice, SCHED_FIFO, SCHED_RR
├── 02_preempt_rt/   → Apply PREEMPT_RT patch to BBB kernel
├── 03_latency/      → Measure scheduling latency (before/after RT)
├── 04_rt_tasks/     → Write RT tasks: mlockall, priority inheritance
└── 05_pru/          → BBB PRU: hard real-time at 5ns precision
```

## Build & Run Order

```bash
# Each folder:
make cross                              # build for BBB
scp <binary>-bbb debian@192.168.7.2:~/ # copy to BBB
ssh debian@192.168.7.2                 # login
sudo ./<binary>                        # run (needs root for RT)
```

## Key Concepts Per Step

| Step | What You Learn |
|------|----------------|
| 01   | How Linux decides which task runs and when |
| 02   | How to patch the kernel for real-time guarantees |
| 03   | How to measure and prove latency improvements |
| 04   | How to write correct RT tasks in C |
| 05   | How to use PRU for tasks Linux simply cannot do |
