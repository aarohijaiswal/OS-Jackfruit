# Lightweight Container Runtime (OS Project)

##  Overview

This project implements a **lightweight container runtime similar to Docker** using Linux system calls and kernel modules.

It supports:
- Process isolation using namespaces  
- Multi-container management  
- CLI-based control using IPC  
- Logging using producer-consumer model  
- Kernel-level memory monitoring  
- Scheduler behavior analysis  

---

## Features

###  Task 1: Container Runtime
- Containers created using `clone()`
- Isolation using:
  - PID namespace  
  - UTS namespace  
  - Mount namespace  
- Filesystem isolation using `chroot()`

---

###  Task 2: CLI + IPC
- CLI commands:
  - `start`
  - `stop`
  - `ps`
- Communication using **FIFO (named pipe)**

---

###  Task 3: Logging System
- Container output captured via **pipe**
- Stored using **producer-consumer model**
- Synchronization using:
  - mutex  
  - condition variables  
- Logs stored in:
  - `alpha.log`
  - `beta.log`

---

###  Task 4: Kernel Module
- Custom kernel module (`monitor.ko`)
- Tracks memory usage (RSS)
- Uses `ioctl` for communication
- Implements:
  - Soft limit → warning  
  - Hard limit → kill process  

---

###  Task 5: Scheduler Experiments

#### Workloads
- **CPU-bound (`cpu_hog`)** → infinite loop  
- **IO-bound (`io_pulse`)** → uses sleep  

####  Experiments

**Experiment 1: CPU vs CPU (different nice values)**  
- nice = 0 → higher CPU  
- nice = 10 → lower CPU  

**Observation:**  
Lower nice value gets more CPU → higher priority.

---

**Experiment 2: CPU vs IO**  
- CPU-bound → high CPU usage  
- IO-bound → low CPU usage  

**Observation:**  
IO processes yield CPU, scheduler favors CPU-bound tasks.

---

**Experiment 3: Equal priority**  
- Both processes share CPU fairly  

**Observation:**  
Linux scheduler ensures fairness.

---

###  Task 6: Cleanup & Control
- `stop <id>` → stops container  
- `exit` → cleans all containers  
- Removes FIFO  
- Kills all running processes  

---

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








