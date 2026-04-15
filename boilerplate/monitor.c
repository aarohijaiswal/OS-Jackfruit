#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/timer.h>

#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"

struct process_info {
    pid_t pid;
    int soft_limit;
    int hard_limit;
    int warned;
    struct process_info *next;
};

static struct process_info *head = NULL;
static int major;

// ================= IOCTL =================
long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct monitor_args args;
    struct process_info *new;

    if (copy_from_user(&args, (struct monitor_args*)arg, sizeof(args)))
        return -EFAULT;

    new = kmalloc(sizeof(struct process_info), GFP_KERNEL);

    new->pid = args.pid;
    new->soft_limit = args.soft_limit;
    new->hard_limit = args.hard_limit;
    new->warned = 0;
    new->next = head;
    head = new;

    printk(KERN_INFO "[monitor] Registered PID %d (soft=%d, hard=%d)\n",
           args.pid, args.soft_limit, args.hard_limit);

    return 0;
}

// ================= FILE OPS =================
static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
};

// ================= MEMORY CHECK =================
void check_memory(void) {
    struct process_info *curr = head;

    while (curr) {
        struct task_struct *task = pid_task(find_vpid(curr->pid), PIDTYPE_PID);

        if (task && task->mm) {
            long rss = get_mm_rss(task->mm) * 4;

            printk(KERN_INFO "[monitor] PID %d RSS: %ld KB\n", curr->pid, rss);

            if (rss > curr->soft_limit && !curr->warned) {
                printk(KERN_WARNING "[monitor] SOFT limit exceeded PID %d\n", curr->pid);
                curr->warned = 1;
            }

            if (rss > curr->hard_limit) {
                printk(KERN_ALERT "[monitor] HARD kill PID %d\n", curr->pid);
                send_sig(SIGKILL, task, 0);
            }
        }

        curr = curr->next;
    }
}

// ================= TIMER =================
static struct timer_list my_timer;

static void timer_callback(struct timer_list *t) {
    check_memory();
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(3000));
}

// ================= INIT =================
static int __init monitor_init(void) {
    printk(KERN_INFO "[monitor] Module loaded\n");

    major = register_chrdev(0, DEVICE_NAME, &fops);

    printk(KERN_INFO "Create device:\n");
    printk(KERN_INFO "mknod /dev/container_monitor c %d 0\n", major);

    timer_setup(&my_timer, timer_callback, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(3000));

    return 0;
}

// ================= EXIT =================
static void __exit monitor_exit(void) {
    del_timer(&my_timer);
    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "[monitor] Module unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");