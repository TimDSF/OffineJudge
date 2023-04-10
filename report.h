#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int report_cpu_usage(char *cgroup, char *name, char *out){
    int fd;
    // FILE *fp;
    char path[256];
    char outpath[256];
    char buf[1024];
    int len;
    sprintf(path, "%s/cpu.stat", cgroup);
    // sprintf(outpath, "%s/%s.txt", output, name);


    if ((fd = open(path, O_RDONLY)) < 0) {
        perror("open");
        exit(1);
    }

    if ((len = read(fd, buf, sizeof(buf))) < 0) {
        perror("read");
        exit(1);
    }

    // if ((fp = fopen(outpath, "a")) < 0) {
    //     perror("output file open");
    //     exit(1);
    // }

    buf[len] = '\0';

    char *ptr = strtok(buf, "\n");
    long u1 = -1, u2 = -1, u3 = -1;
    while (ptr != NULL) {
        if (u1 == -1) {
            u1 = atoi(ptr + 12);
        } else if (u2 == -1) {
            u2 = atoi(ptr + 11);
        } else {
            u3 = atoi(ptr + 13);
        }
        ptr = strtok(NULL, "\n");
    }
    printf("\t\t\t\tcpu: (%.2f%) %d %d %d\n", (float) u2 / u1 * 100, u1, u2, u3);
    // fprintf(fp, "%s", buf);

    close(fd);
    // fclose(fp);

    return 0;
}

int report_memory_usage(char *cgroup, char *name, char *out){
    int fd;
    // FILE *fp;
    char path[256];
    char outpath[256];
    char buf[1024];
    int len;
    sprintf(path, "%s/memory.current", cgroup);
    // sprintf(outpath, "%s/%s.txt", output, name);

    if ((fd = open(path, O_RDONLY)) < 0) {
        perror("open");
        exit(1);
    }

    if ((len = read(fd, buf, sizeof(buf))) < 0) {
        perror("read");
        exit(1);
    }

    // if ((fp = fopen(outpath, "a")) < 0) {
    //     perror("output file open");
    //     exit(1);
    // }

    buf[strlen(buf)-1] = '\0';
    printf("\t\t\t\tmemory: %d\n", atoi(buf)); fflush(stdout);
    // fprintf(fp, "%s", buf);

    close(fd);
    // fclose(fp);

    return 0;
}
