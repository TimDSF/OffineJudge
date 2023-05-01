# Control-Group-Based Offline Judge System  

## Introduction

This is the final project for SP23 CSE 522S at Washington University in St. Louis. It is a local judge system implemented in C using Control-Group v2, tested on Raspberry Pi 3B+.

## Directory
- .
  - host.h: the header containing the host, driver, and monitor functionalities
  - main.c: the main program
  - README.txt: readme file
  - report.h: the header containing the interface to read the collected statistics
- demo
  - stats.numbers: contains demo statistics
- executables1
  - fork: a simple fork bomb
  - mem1: allocate approximately 200 MB of memory
  - mem2: allocate approximately 300 MB of memory
  - rand: performing random float operations
  - sleep1.sh: shell code that sleep for 1 second
  - sleep2.sh: shell code that sleep for 2 seconds
  - stats.csv: raw output file
- executables2
  - hostname.sh: shell code that tries to change the host name
  - reboot.sh: shell code that tries reboot the system
- executables3
  - sudo_hostname.sh: change the hostname with sudo
  - sudo_reboot.sh: reboot with sudo
  - *: raw c code for the executable

## Execution
- sample command
  - clear; gcc main.c -o main; sudo ./main 1500 50 200 1 executables2/
