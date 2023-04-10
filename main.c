#define _OPEN_SYS_ITOA_EXT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#include "host.h"

#define N_CORES 4

enum RETURN {
  SUCCESS,
  UNKNOEN,
  TOO_MANY_CORES_REQUESTED,
};

//
void environmental_setup() {
  int i, pid;
  char cmd[100];
  FILE * f;

  f = fopen("/sys/fs/cgroup/cgroup.procs", "r");

  sprintf(cmd, "echo \"+cpuset +cpu +io +memory +pids\" > /sys/fs/cgroup/cgroup.subtree_control");
  for (i = 0; i < N_CORES; ++ i) {
    sprintf(cmd, "mkdir /sys/fs/cgroup/core%d", i); system(cmd);
    sprintf(cmd, "echo %d > /sys/fs/cgroup/core%d/cpuset.cpus", i, i); system(cmd);
  }
  while(fscanf(f, "%d", &pid) != EOF) {
    sprintf(cmd, "echo %d > /sys/fs/cgroup/core0/cgroup.procs", pid); system(cmd);
  }
}

// argv[1]: # cores to use
// argv[2...]: directory for the executables for each core 
int main(int argc, char* argv[]) {
  int status;
  int i, n_cores, res;
  char cmd[100];
  
  environmental_setup();

  n_cores = atoi(argv[1]);
  if (n_cores + 1 > N_CORES) {
    printf("Too many cores requested. Available %d cores\n.", N_CORES - 1); fflush(stdout);
    return TOO_MANY_CORES_REQUESTED;
  }

  for (i = 1; i <= n_cores; ++ i) {
    sprintf(cmd, "chmod 744 %s/*", argv[i + 1]); system(cmd);
    if (fork() == 0) {
      printf("1\n");
      host(i, argv[i + 1]);
      printf("1\n");
      exit(0);
    }
  }
  for (i = 1; i <= n_cores; ++ i) {
    wait(&status);
  }
  exit(0);
}
