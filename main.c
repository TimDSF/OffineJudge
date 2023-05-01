#define _OPEN_SYS_ITOA_EXT
#define N_CORES sysconf(_SC_NPROCESSORS_ONLN)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#include "host.h"

// Return values
enum RETURN {
  SUCCESS,
  UNKNOEN,
  TOO_MANY_CORES_REQUESTED,
  N_CORES_MISMATCH,
};

// The usage function
void usage(char * prog) {
  printf("\033[0;33mUsage: %s <TL> <SR> <ML> <NC> <Dir_1> ... <Dir_NC>\033[0m\n", prog);
  printf("\033[0;33m  TL: time limit (unit: ms)\033[0m\n");
  printf("\033[0;33m  SR: sample rate (unit: ms)\033[0m\n");
  printf("\033[0;33m  ML: memory limit (unit: MB)\033[0m\n");
  printf("\033[0;33m  NC: number of cores running, max: %d\033[0m\n", N_CORES - 1);
  printf("\033[0;33m  <Dir_1> ... <Dir_NC>: directories containing the executables for each core\033[0m\n");
}

// The environmental setup function. It will create and assign one control group per CPU core, add the cpuset, cpu, io, memory, and pid field to the subtree control of the base cgroup, then, move all the existing processes to the cgroup/core0.
void environmental_setup() {
  int i, pid;
  char cmd[100];
  FILE * f;

  f = fopen("/sys/fs/cgroup/cgroup.procs", "r");

  sprintf(cmd, "echo \"+cpuset +cpu +io +memory +pids\" > /sys/fs/cgroup/cgroup.subtree_control");
  for (i = 0; i < N_CORES; ++ i) {
    sprintf(cmd, "(mkdir /sys/fs/cgroup/core%d) 2> /dev/null", i); system(cmd);
    sprintf(cmd, "echo %d > /sys/fs/cgroup/core%d/cpuset.cpus", i, i); system(cmd);
  }
  while(fscanf(f, "%d", &pid) != EOF) {
    sprintf(cmd, "(echo %d > /sys/fs/cgroup/core0/cgroup.procs) 2> /dev/null", pid); system(cmd);
  }
}

// The main function
//   argv[1]: time limit, unit millisecond
//   argv[2]: sample rate, unit millisecond
//   argv[3]: memory limit, unit MB
//   argv[4]: # cores to use
//   argv[5...]: directory for the executables for each core 
int main(int argc, char* argv[]) {
  int status;
  int i, n_cores, res;
  char cmd[100];
  
  if (argc < 6){
    usage(argv[0]);
    return -1;
  }

  n_cores = atoi(argv[4]);

  // when the user requests too much cores
  if (n_cores + 1 > N_CORES) {
    printf("\033[0;33mToo many cores requested. Available %d cores\033[0m\n", N_CORES - 1); fflush(stdout);
    return TOO_MANY_CORES_REQUESTED;
  }
  // when the user does not specify enough directory to be executed on each core
  if (argc != n_cores + 5) {
    printf("\033[0;33mNumber of directories does not match with number of cores. Cores requested: %d, directories provided: %d\033[0m\n", n_cores, argc - 5); fflush(stdout);
    return N_CORES_MISMATCH; 
  }

  // set up the environment
  environmental_setup();

  // bring up a host program for each core on their assigned core respectively
  for (i = 1; i <= n_cores; ++ i) {
    sprintf(cmd, "chmod 744 %s/*", argv[i + 4]); system(cmd);
    if (fork() == 0) {
      host(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), i, argv[i + 4]);
      exit(0);
    }
  }

  // wait for each of the forked child
  for (i = 1; i <= n_cores; ++ i) {
    wait(&status);
  }

  exit(SUCCESS);
}