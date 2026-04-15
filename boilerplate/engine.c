#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/resource.h>

#include "monitor_ioctl.h"

#define STACK_SIZE (1024 * 1024)
#define MAX_CONTAINERS 10
#define FIFO_PATH "/tmp/engine_cmd"
#define BUFFER_SIZE 10

typedef struct {
    char id[32];
    pid_t pid;
    int active;
    time_t start_time;
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

char stacks[MAX_CONTAINERS][STACK_SIZE];

char buffer[BUFFER_SIZE][256];
int in = 0, out = 0, count_buf = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

typedef struct {
    char rootfs[100];
    int write_fd;
    int nice_val;
    char cmd[100];
} container_args;

// ================= CONTAINER =================
int container_main(void *arg) {
    container_args *args = (container_args*)arg;

    setbuf(stdout, NULL);

    // Apply priority
    setpriority(PRIO_PROCESS, 0, args->nice_val);

    dup2(args->write_fd, STDOUT_FILENO);
    dup2(args->write_fd, STDERR_FILENO);
    close(args->write_fd);

    printf("[Container] Starting...\n");

    chroot(args->rootfs);
    chdir("/");

    // FIX: mount proc
    mount("proc", "/proc", "proc", 0, NULL);

    printf("[Container] Running %s\n", args->cmd);

    execl(args->cmd, args->cmd, NULL);

    perror("exec failed");
    return 1;
}

// ================= PRODUCER =================
void* producer(void *arg) {
    int fd = *(int*)arg;
    char temp[256];

    while (1) {
        int n = read(fd, temp, sizeof(temp)-1);
        if (n <= 0) break;

        temp[n] = '\0';

        pthread_mutex_lock(&mutex);
        while (count_buf == BUFFER_SIZE)
            pthread_cond_wait(&not_full, &mutex);

        strcpy(buffer[in], temp);
        in = (in + 1) % BUFFER_SIZE;
        count_buf++;

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// ================= CONSUMER =================
void* consumer(void *arg) {
    char *filename = (char*)arg;
    FILE *log = fopen(filename, "a");

    while (1) {
        pthread_mutex_lock(&mutex);
        while (count_buf == 0)
            pthread_cond_wait(&not_empty, &mutex);

        char data[256];
        strcpy(data, buffer[out]);
        out = (out + 1) % BUFFER_SIZE;
        count_buf--;

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        fprintf(log, "%s", data);
        fflush(log);
    }
    return NULL;
}

// ================= START =================
void start_container(char *id, char *rootfs, char *cmd, int nice_val) {

    int pipefd[2];
    pipe(pipefd);

    int idx = container_count;

    container_args *args = malloc(sizeof(container_args));
    strcpy(args->rootfs, rootfs);
    strcpy(args->cmd, cmd);
    args->write_fd = pipefd[1];
    args->nice_val = nice_val;

    pid_t pid = clone(container_main,
                      stacks[idx] + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
                      args);

    close(pipefd[1]);

    containers[idx].pid = pid;
    containers[idx].active = 1;
    containers[idx].start_time = time(NULL);
    strcpy(containers[idx].id, id);
    container_count++;

    printf("[Supervisor] Started %s (PID %d, nice=%d)\n", id, pid, nice_val);

    // Kernel module registration
    int fd = open("/dev/container_monitor", O_RDWR);
    if (fd >= 0) {
        struct monitor_args margs = {pid, 40000, 60000};
        ioctl(fd, REGISTER_PROCESS, &margs);
        close(fd);
    }

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, &pipefd[0]);

    char *logfile = malloc(100);
    sprintf(logfile, "%s.log", id);

    pthread_create(&cons, NULL, consumer, logfile);
}

// ================= STOP =================
void stop_container(char *id) {
    for (int i = 0; i < container_count; i++) {
        if (strcmp(containers[i].id, id) == 0 && containers[i].active) {
            kill(containers[i].pid, SIGKILL);
            containers[i].active = 0;
            printf("[Supervisor] Stopped %s\n", id);
        }
    }
}

// ================= LIST =================
void list_containers() {
    printf("\n===== CONTAINERS =====\n");

    for (int i = 0; i < container_count; i++) {
        long uptime = time(NULL) - containers[i].start_time;

        printf("ID: %s | PID: %d | %s | Uptime: %ld sec\n",
               containers[i].id,
               containers[i].pid,
               containers[i].active ? "Running" : "Stopped",
               uptime);
    }

    printf("======================\n");
}

// ================= CLEANUP =================
void cleanup() {
    for (int i = 0; i < container_count; i++) {
        if (containers[i].active) {
            kill(containers[i].pid, SIGKILL);
        }
    }
    unlink(FIFO_PATH);
    printf("[Supervisor] Cleaned up\n");
}

// ================= SUPERVISOR =================
void run_supervisor() {
    unlink(FIFO_PATH);
    mkfifo(FIFO_PATH, 0666);

    char buffer_cmd[256];

    printf("[Supervisor] Running...\n");

    while (1) {
        int fd = open(FIFO_PATH, O_RDONLY);
        int n = read(fd, buffer_cmd, sizeof(buffer_cmd)-1);
        close(fd);

        if (n <= 0) continue;

        buffer_cmd[n] = '\0';

        char cmd[20], id[32], rootfs[100], prog[100];
        int nice_val = 0;

        int parsed = sscanf(buffer_cmd, "%s %s %s %s %d",
                            cmd, id, rootfs, prog, &nice_val);

        if (parsed < 5) nice_val = 0;

        if (strcmp(cmd, "start") == 0) {
            start_container(id, rootfs, prog, nice_val);
        }
        else if (strcmp(cmd, "stop") == 0) {
            stop_container(id);
        }
        else if (strcmp(cmd, "ps") == 0) {
            list_containers();
        }
        else if (strcmp(cmd, "exit") == 0) {
            cleanup();
            break;
        }
    }
}

// ================= CLI =================
void send_command(char *cmd) {
    int fd = open(FIFO_PATH, O_WRONLY);
    write(fd, cmd, strlen(cmd));
    close(fd);
}

// ================= MAIN =================
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage:\n");
        printf("./engine supervisor\n");
        printf("./engine start <id> <rootfs> <cmd> <nice>\n");
        printf("./engine stop <id>\n");
        printf("./engine ps\n");
        printf("./engine exit\n");
        return 1;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        run_supervisor();
    }
    else if (strcmp(argv[1], "start") == 0) {
        int nice_val = (argc > 5) ? atoi(argv[5]) : 0;

        char cmd[200];
        sprintf(cmd, "start %s %s %s %d",
                argv[2], argv[3], argv[4], nice_val);

        send_command(cmd);
    }
    else if (strcmp(argv[1], "stop") == 0) {
        char cmd[100];
        sprintf(cmd, "stop %s", argv[2]);
        send_command(cmd);
    }
    else if (strcmp(argv[1], "ps") == 0) {
        send_command("ps");
    }
    else if (strcmp(argv[1], "exit") == 0) {
        send_command("exit");
    }

    return 0;
}