#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define N 30000000

int a[N];

int main() {
  char* tmp1 = malloc(N);
  memset(tmp1, 10, sizeof(tmp1));
  usleep(100000);
  
  char* tmp2 = malloc(N);
  memset(tmp2, 10, sizeof(tmp2));
  usleep(100000);
  
  char* tmp3 = malloc(N);
  memset(tmp3, 10, sizeof(tmp3));
  usleep(100000);
  
  char* tmp4 = malloc(N);
  memset(tmp4, 10, sizeof(tmp4));
  usleep(100000);
  
  char* tmp5 = malloc(N);
  memset(tmp5, 10, sizeof(tmp5));
  usleep(100000);
  
  char* tmp6 = malloc(N);
  memset(tmp6, 10, sizeof(tmp6));
  usleep(100000);
  
  while(1);
}
