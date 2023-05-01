#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <math.h>

#include "report.h"

int running = 0;
long memory_limit = 200 * 1024 * 1024;
struct timespec ts1, ts2;
struct timespec sample_rate = {0, 100000000}, time_limit = {1, 500000000};
struct timespec next_sample;

static int * program_pid = 0;
static char * prog;

// helper function for calculating the difference in time between two timespecs
void timespec_diff(struct timespec * a, struct timespec * b, struct timespec * res) {
  if (b->tv_nsec < a->tv_nsec) {
    res->tv_sec = b->tv_sec - a->tv_sec - 1;
    res->tv_nsec = 1000000000 + b->tv_nsec - a->tv_nsec;
  } else {
    res->tv_sec = b->tv_sec - a->tv_sec;
    res->tv_nsec = b->tv_nsec - a->tv_nsec;
  }
}

// comparator for two timespecs
int timespec_cmp(struct timespec * a, struct timespec * b) {
  if (a->tv_sec == a->tv_sec && a->tv_nsec == b->tv_nsec) return 0;
  if (a->tv_sec < b->tv_sec || a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec) return -1;
  return 1;
}

// the function that collects the data at each sample
unsigned long sample(char* dir, FILE * stats, struct timespec duration) {
  FILE * fp;
  float cpu, time ;
  char mem[50];
  unsigned long memory;

  cpu = report_cpu_usage(dir);
  memory = report_memory_usage(dir);

  if (memory < 1024) {
    sprintf(mem, "%ld B", memory);
  } else if (memory < 1024 * 1024){
    sprintf(mem, "%ld KB", memory / 1024);
  } else if (memory < 1024 * 1024 * 1024){
    sprintf(mem, "%ld MB", memory / 1024 / 1024);
  } else {
    sprintf(mem, "%ld GB", memory / 1024 / 1024 / 1024);
  }

  if (cpu > 100) cpu = 100;
  if (isnan(cpu)) cpu = 0;

  time = duration.tv_sec + (float) duration.tv_nsec / 1000000000;

  printf("\033[0;33m[%d]     sampling %d...\t\t%.3fs\t%2.1f%%\t%s\033[0m\n", getpid(), *program_pid, time, cpu, mem); fflush(stdout);
  fprintf(stats, "%s,%.3fs,%2.1f%%,%s\n", prog, time, cpu, mem);

  return memory;
}

// helper function that kills all the process inside the specified container
void kill_core(int core_id) {
  char buf[100];
  int host = -1, pid;
  FILE * fp;

  sprintf(buf, "/sys/fs/cgroup/core%d/cgroup.procs", core_id);
  fp = fopen(buf, "r");

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    pid = atoi(buf);

    if (host == -1) {  // skip the host program, which is the one with the smallest pid
        host = 0;
    } else {
        kill(pid, SIGKILL);
    }
  }
  fclose(fp);
}

// signal handler for SIGUSR1: start the monitor and record the next_sample time
void monitor_start() {
  clock_gettime(CLOCK_REALTIME, &ts1);
  next_sample = ts1;
  printf("\033[0;33m[%d]   running...\033[0m\n", getpid()); fflush(stdout);
  running = 1;
}

// signal handler for SIGUSR2: pause the monitor and report 
void monitor_stop() {
  struct timespec duration;
  clock_gettime(CLOCK_REALTIME, &ts2);
  timespec_diff(&ts1, &ts2, &duration);

  printf("\033[1;32m[%d] finished %ld.%09ld...\033[0m\n", getpid(), duration.tv_sec, duration.tv_nsec); fflush(stdout);
  running = 0;
}

// signal handler for SIGUSR1: start the monitor and record the next_sample time
void monitor_terminate() {
  printf("\033[0;33m[%d] Monitor Terminated\033[0m\n", getpid()); fflush(stdout);
  exit(0);
}

// The monitor process. It will trap itself in an infinite loop. When there are programs running, it will use delta timing to sample the statistics of interest. At the same time, it will stop the program when we have time_limit_exceeds or memory_limit_exceeds.
void monitor(int core_id, char* out) {
  char dir[100], path[100], cg_path[100], cmd[100], buf[100];
  struct timespec cur_time, duration;
  int host, pid;
  unsigned long cur_mem;
  FILE * fp;
  FILE * stats;

  sprintf(path, "%s/stats.csv", out);
  stats = fopen(path, "w");
  fprintf(stats, "file,time,cpu,memory\n");

  sprintf(cmd, "echo %d > /sys/fs/cgroup/core0/cgroup.procs", getpid()); system(cmd);
  printf("\033[1;32m[%d] Monitor Started\033[0m\n", getpid()); fflush(stdout);

  // set up the signal handlers
  signal(SIGUSR1, monitor_start);
  signal(SIGUSR2, monitor_stop);
  signal(SIGINT, monitor_terminate);

  // while loop for the monitor
  while (1) {
    // when there are programming running
    if (running) {
      clock_gettime(CLOCK_REALTIME, &cur_time);
      // check by delta timing, continuous if we have not encounter the next sample time
      if (cur_time.tv_sec < next_sample.tv_sec || cur_time.tv_sec == next_sample.tv_sec && cur_time.tv_nsec < next_sample.tv_nsec) {
        continue;
      }
      
      // calculate the next_sample time
      next_sample.tv_nsec += sample_rate.tv_nsec;
      next_sample.tv_sec += next_sample.tv_nsec / 1000000000;
      next_sample.tv_nsec %= 1000000000;
      
      // kill if memory_limit_exceeds
      sprintf(cg_path, "/sys/fs/cgroup/core%d", core_id);
      cur_mem = sample(cg_path, stats, duration);
      if (cur_mem > memory_limit){
        printf("\033[1;31m[%d] Exceed Memory Max: %d\033[0m\n", getpid(), *program_pid); fflush(stdout);
        cur_mem = 0;

        kill_core(core_id);
        monitor_stop();
      }

      // kill if time_limit_exceeds
      timespec_diff(&ts1, &cur_time, &duration);
      if (timespec_cmp(&duration, &time_limit) == 1) {
        printf("\033[1;31m[%d] Timeout: %d\033[0m\n", getpid(), *program_pid); fflush(stdout);

        kill_core(core_id);
        monitor_stop();
      }
    }
  }

  fclose(stats);
}

// The driver process. It will iterate over all the executables in the specified directory. For each executable, it will run the program, signaling to the monitor the start and stop of the program.
void driver(int core_id, char* dir) {
  int status, pid, ppid;
  DIR *dp = NULL;
  struct dirent *entry = NULL;
  char cmd[100];

  // put the driver to the desired container
  ppid = getppid();
  sprintf(cmd, "echo %d > /sys/fs/cgroup/core%d/cgroup.procs", getpid(), core_id); system(cmd);
  printf("\033[0;33m[%d] Driving core %d\033[0m\n", getpid(), core_id); fflush(stdout);

  // iterate through the directory
  dp = opendir(dir);
  if (dp != NULL) {
    while ((entry = readdir(dp))) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      
      
      pid = fork();
      if (pid == 0) {  // for a child for monitoring the grandchild, and signaling the monitor
        sprintf(cmd, "%s/%s", dir, entry->d_name); fflush(stdout);

        kill(ppid, SIGUSR1);  // signal the start of the program to the monitor
        if (fork() == 0) {  // fork a grandchild
          setuid(1000);  // get uid to 1000 to drop the sudo privilege
          system(cmd);  // run the program with normal privilege
          exit(0);
        } else {  // child wait on the grandchild to finish
          wait(&status);
        }
        kill(ppid, SIGUSR2);  // signal the stop of the program to the monitor
        exit(0);
      } else {  // driver waiting on the child
        printf("\033[0;36m==========================================================\033[0m\n"); fflush(stdout);
        printf("\033[1;32m[%d] process %s runs at %d\033[0m\n", getpid(), entry->d_name, pid); fflush(stdout);
        
        *program_pid = pid;
        strcpy(prog, entry->d_name);
        
        wait(&status);
      }
      usleep(1000000);
    }
    closedir(dp);
  }
  kill(ppid, SIGINT);
}

// The host program. It will set up the shared memory between the monitor and driver program. Then, it will fork a driver and becomes the monitor program.
int host(long tl, long sr, long ml, int core_id, char* dir) {
  program_pid = mmap(NULL, sizeof *program_pid, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  prog = mmap(NULL, sizeof *prog, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  int status;
  if (tl != -1) {
    time_limit.tv_sec = tl / 1000;
    time_limit.tv_nsec = tl % 1000 * 1000 * 1000;
  }
  if (sr != -1) {
    sample_rate.tv_sec = sr / 1000;
    sample_rate.tv_nsec = sr % 1000 * 1000 * 1000;
  }
  if (ml != -1) {
    memory_limit = ml * 1024 * 1024;
  }

  printf("\033[1;32m[%d] Host Started\033[0m\n", getpid()); fflush(stdout);
  if (fork() != 0) {  // the host becomes the monitor
    monitor(core_id, dir);
    wait(&status);
  } else {  // the forked child becomes the driver
    usleep(500000);
    driver(core_id, dir);
  }

  return 0;
}
