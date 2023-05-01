#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

unsigned long prev_time = 0;
float prev_uptime = 0;

// read the cpu percentage usage
float report_cpu_usage(char *cgroup){
  int fd, len, host = -1, pid;
  float uptime;
  int total_time = 0;
  unsigned long utime, stime;
  FILE * fp;
  FILE * file;
  char path[256], buf[1024];
  float ratio = 0.0;

  // read the uptime
  fp = fopen("/proc/uptime", "r");
  fscanf(fp, "%*f %f", &uptime);
  fclose(fp);

  // iterate through all processes in the cgroup
  sprintf(path, "%s/cgroup.procs", cgroup);
  fp = fopen(path, "r");

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    if (host == -1) {  // skip the host program
      host = 0;
      continue;
    }
    pid = atoi(buf);

    // read the user time and system time of the program
    sprintf(path, "/proc/%d/stat", pid);
    file = fopen(path, "r");
    fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
    total_time += utime + stime;
    fclose(file);
  }
  fclose(fp);

  // calculate the instantaneous cpu usage (average cpu usage since last sample) of all the processes in the cgroup
  ratio = (float) 100 * (total_time - prev_time) / (uptime - prev_uptime) / sysconf(_SC_CLK_TCK);

  // record the current status for next sample
  prev_time = total_time;
  prev_uptime = uptime;

  return ratio;
}

// read the memory usage
unsigned long report_memory_usage(char *cgroup){
  FILE *fp;
  char path[256], buf[1024];
  FILE *file;
  int len, pid, host = -1;
  long vsize, total = 0;

  // iterate through all processes in the cgroup
  sprintf(path, "%s/cgroup.procs", cgroup);
  fp = fopen(path, "r");

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    if (host == -1) {  // skip the host
      host = 0;
      continue;
    }
    pid = atoi(buf);

    // read the memory usage of the program
    sprintf(path, "/proc/%d/stat", pid);
    file = fopen(path, "r");
    fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %lu", &vsize);
    total += vsize;
    fclose(file);
  }

  fclose(fp);

  return total;
}
