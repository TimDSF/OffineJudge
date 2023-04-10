#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#include "report.h"

int running = 0;
struct timespec ts1, ts2;
struct timespec sample_rate = {0, 100000000}, time_limit = {1, 500000000};
struct timespec next_sample;

static int *program_pid = 0;
static char program_running[100];

void timespec_diff(struct timespec * a, struct timespec * b, struct timespec * res) {
  if (b->tv_nsec < a->tv_nsec) {
    res->tv_sec = b->tv_sec - a->tv_sec - 1;
    res->tv_nsec = 1000000000 + b->tv_nsec - a->tv_nsec;
  } else {
    res->tv_sec = b->tv_sec - a->tv_sec;
    res->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
}

int timespec_cmp(struct timespec * a, struct timespec * b) {
  if (a->tv_sec == a->tv_sec && a->tv_nsec == b->tv_nsec) return 0;
  if (a->tv_sec < b->tv_sec || a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec) return -1;
  return 1;
}

void sample(char* dir, char* out) {
  printf("[%d]     sampling (%d)...\n", getpid(), *program_pid); fflush(stdout);
  report_cpu_usage(dir, program_running, out);
  report_memory_usage(dir, program_running, out);
}


void monitor_start() {
  clock_gettime(CLOCK_REALTIME, &ts1);
  next_sample = ts1;
  printf("[%d]     running...\n", getpid()); fflush(stdout);
  running = 1;
}

void monitor_stop() {
  struct timespec duration;
  clock_gettime(CLOCK_REALTIME, &ts2);
  timespec_diff(&ts1, &ts2, &duration);
  printf("[%d]     finished %ld.%09ld...\n", getpid(), duration.tv_sec, duration.tv_nsec); fflush(stdout);
  running = 0;
}

void monitor_terminate() {
  printf("[%d] Monitor Terminated\n", getpid()); fflush(stdout);
  exit(0);
}

void monitor(int core_id, char* out) {
  char dir[100];
  struct timespec cur_time, duration;
  char cmd[100];
  char cg_path[100];
  sprintf(cmd, "echo %d > /sys/fs/cgroup/core0/cgroup.procs", getpid()); system(cmd);

  printf("[%d] Monitor Started\n", getpid()); fflush(stdout);
  signal(SIGUSR1, monitor_start);
  signal(SIGUSR2, monitor_stop);
  signal(SIGINT, monitor_terminate);

  while (1) {
    if (running) {
      clock_gettime(CLOCK_REALTIME, &cur_time);
      if (cur_time.tv_sec < next_sample.tv_sec || cur_time.tv_sec == next_sample.tv_sec && cur_time.tv_nsec < next_sample.tv_nsec) {
        continue;
      }
      timespec_diff(&ts1, &cur_time, &duration);
      if (timespec_cmp(&duration, &time_limit) == 1) {
        printf("[%d]  Timeout: %d\n", getpid(), *program_pid); fflush(stdout);
        kill(*program_pid, SIGKILL);
        running = 0;
      }
      next_sample.tv_nsec += sample_rate.tv_nsec;
      next_sample.tv_sec += next_sample.tv_nsec / 1000000000;
      next_sample.tv_nsec %= 1000000000;
      sprintf(cg_path, "/sys/fs/cgroup/core%d", core_id);
      sample(cg_path, out);
    }
  }
}

void driver(int core_id, char* dir) {
  int status, pid, ppid;
  DIR *dp = NULL;
  struct dirent *entry = NULL;
  char cmd[100];

  ppid = getppid();
  sprintf(cmd, "echo %d > /sys/fs/cgroup/core%d/cgroup.procs", getpid(), core_id); system(cmd);

  printf("[%d] Driving core %d\n", getpid(), core_id); fflush(stdout);
  dp = opendir(dir);
  if (dp != NULL) {
    while ((entry = readdir(dp))) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      
      pid = fork();
      if (pid == 0) {
        sprintf(cmd, "%s/%s", dir, entry->d_name); fflush(stdout);
        kill(ppid, SIGUSR1);
        system(cmd);
        kill(ppid, SIGUSR2);
        exit(0);
      } else {
        printf("---------------------------------------------\n"); fflush(stdout);
        printf("[%d]   process %s runs at %d\n", getpid(), entry->d_name, pid); fflush(stdout);
        *program_pid = pid;
        wait(&status);
      }
      usleep(1000000);
    }
    closedir(dp);
  }
  kill(ppid, SIGINT);
}

int host(int core_id, char* dir) {
  program_pid = mmap(NULL, sizeof *program_pid, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  int status;

  printf("[%d] Host Started\n", getpid()); fflush(stdout);
  if (fork() != 0) {
    monitor(core_id, dir);
    wait(&status);
  } else {
    usleep(500000);
    driver(core_id, dir);
  }

  return 0;
}