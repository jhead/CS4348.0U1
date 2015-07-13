/*
 * CS4348 - Project 1
 * Justin Head (jxh122430@utdallas.edu)
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFSIZE 25

void childOne();
void childTwo();

void displayCow();
int validateValues();
int strtoi(char*);
void itostr(int, char*);

int pfd[2];
int pid = -1;
int cow;

/**
 * Entry point
 */
int main() {
  int pidOne, pidTwo;

  // Create pipe for IPC
  if (pipe(pfd) < 0) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  // Fork first child process
  cow = 10;
  pid = pidOne = fork();

  if (pidOne == 0) {
      childOne();
      return 0;
  } else if (pidOne < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  // Display cow value in parent
  displayCow();

  // Read child cow value from pipe and verify that both are equal
  validateValues();

  // Read and display child 1 message
  char buf[BUFSIZE];

  if (read(pfd[0], buf, BUFSIZE) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  printf("Message Received: %s", buf);

  // Read updated cow value from child and compare
  displayCow();
  validateValues();

  // Fork second child process
  pidTwo = fork();
  if (pidTwo == 0) {
    childTwo();
    return 0;
  } else if (pidTwo < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  // Wait for both child procs to terminate
  int statusOne, statusTwo;

  if (waitpid(pidOne, &statusOne, 0) < 0) {
    perror("waitpid");
    exit(EXIT_FAILURE);
  }

  if (waitpid(pidTwo, &statusTwo, 0) < 0) {
    perror("waitpid");
    exit(EXIT_FAILURE);
  }

  return 0;
}

/**
 * Child process #1
 */
void childOne() {
  // Display cow value (in child)
  printf("Cow: %i (Child)\n", cow);

  // Stringify cow value
  char str[BUFSIZE];
  itostr(cow, str);

  // Write cow value to pipe for verification
  if (write(pfd[1], str, BUFSIZE) < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }

  // Write message
  if (write(pfd[1], "Hello from child 1!\n", BUFSIZE) < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }

  usleep(100);

  // Update child cow value and write to pipe for verification
  cow = 1000;
  itostr(cow, str);

  if (write(pfd[1], str, BUFSIZE) < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }

  // Display new cow value (in child)
  displayCow();
}

/**
 * Child process #2 code
 *
 * Replaces program with /bin/ls using no args
 */
void childTwo() {
  char* const env[] = { NULL };
  char* const args[] = { "", NULL };

  if (execve("/bin/ls", args, env) < 0) {
    perror("execve");
    exit(EXIT_FAILURE);
  }
}

/**
 * Displays value of cow based on global pid value
 */
void displayCow() {
  if (pid == 0) {
    printf("Cow: %i (Child)\n", cow);
  } else {
    printf("Cow: %i (Parent)\n", cow);
  }
}

/**
 * Reads int (cow) from pipe and validates it against parent cow value
 */
int validateValues() {
  char buf[BUFSIZE];

  if (read(pfd[0], buf, BUFSIZE) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  int otherCow = strtoi(buf);

  if (cow == otherCow) {
    printf("Parent and child cow values verified\n");
    return 1;
  }

  fprintf(stderr, "Parent and child cow values do not match: %i\n", otherCow);
  return 0;
}

/**
 * Converts the supplied char* (string) to an int
 */
int strtoi(char* buf) {
  return strtol(buf, NULL, 10);
}

/**
 * Converts the supplied int to a string, outputing the data to buf
 */
void itostr(int i, char* buf) {
  sprintf(buf, "%i", i);
}
