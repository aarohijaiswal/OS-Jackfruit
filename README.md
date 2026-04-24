#  Lightweight Container Runtime (OS Mini Project)

## Team Members

- **AADITYA VASHISHT** — PES1UG24CS004  
- **AAROHI JAISWAL** — PES1UG24CS009  

---

##  Project Overview

This project implements a lightweight container runtime using Linux system programming concepts such as namespaces, chroot, IPC, and kernel modules. It provides features like container lifecycle management, logging, memory monitoring, scheduler analysis, and cleanup.

---

## ⚙️ Features Implemented

###  Task 1: Container Runtime
- Containers created using `clone()`
- PID, UTS, and Mount namespaces used
- Filesystem isolation using `chroot()`
- Multiple containers supported

---

###  Task 2: CLI + IPC
- Commands implemented:
  - `start`
  - `stop`
  - `ps`
- Communication via FIFO (`/tmp/engine_cmd`)

---

###  Task 3: Logging System
- Container output captured using pipes
- Producer-consumer model implemented
- Logs stored in files (e.g., `logger.log`)

---

###  Task 4: Kernel Module
- Custom module `monitor.ko`
- Tracks memory usage (RSS)
- Uses `ioctl`
- Soft limit → warning  
- Hard limit → process kill  

---

###  Task 5: Scheduler Analysis

Commands used:

```bash
yes > /dev/null &
yes > /dev/null &
ps -eo pid,ni,pcpu,comm | grep yes
sudo renice -10 -p <PID1>
sudo renice 10 -p <PID2>
watch -n 1 "ps -eo pid,ni,pcpu,comm | grep yes"

###  Task 6: Cleanup & Control
- `stop <id>` → stops container  
- `exit` → cleans all containers  
- Removes FIFO  
- Kills all running processes  

---

### Task 7: Scheduler Verification
Verified scheduler behavior using CPU-bound processes

Observed:
Different NI values
Different %CPU usage

Task 8: System Cleanup & No Zombies

Observation:
No zombie processes remained after cleanup.


##  Project Structure

OS-Jackfruit/
│
├── engine.c
├── monitor.c
├── monitor_ioctl.h
├── Makefile
│
├── rootfs-alpha/
├── rootfs-beta/
│
├── cpu_hog.c
├── io_pulse.c
│
├── alpha.log
├── beta.log








