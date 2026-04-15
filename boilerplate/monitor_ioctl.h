#ifndef MONITOR_IOCTL_H
#define MONITOR_IOCTL_H

#include <linux/ioctl.h>

#define REGISTER_PROCESS _IOW('a', 'a', struct monitor_args)

struct monitor_args {
    int pid;
    int soft_limit;
    int hard_limit;
};

#endif