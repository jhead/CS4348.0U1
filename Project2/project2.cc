/*
 * CS4348 - Project 2
 * Justin Head (jxh122430@utdallas.edu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <map>
#include <vector>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include "project2.h"

int numWorkers;
char* searchKeyword;
char* searchPath;
int chunkSize, lastChunkSize;
std::map<int, int> lineMap;
std::vector<int> results;

// Pipe struct holding two pairs of pipe() file descriptors - one for child-
// bound data, one for parent-bound data. Used to accomplish bi-directional
// communicaiton between parent and child processes.
struct Pipe {
  int toChild[2];
  int toParent[2];
};

std::vector<Pipe> pipes;

/**
 *
 * Program entry point
 */
int main(int argc, char** argv) {
  if (argc < 4) {
    usage();
    return 1;
  }

  int lineCount;

  // Assign values from command line args
  numWorkers = atoi(argv[1]);
  searchKeyword = argv[2];
  searchPath = argv[3];

  // Map and count lines
  lineCount = mapLines(searchPath);

  if (lineCount < 0) {
    exit(1);
  }

  // Clamp maximum number of workers to file line count
  if (numWorkers > lineCount) {
    numWorkers = lineCount;
  }

  // Calculate number of lines for each worker to process
  chunkSize = floor(lineCount / numWorkers);
  lastChunkSize = chunkSize + (lineCount % numWorkers);

  // Initialize other necessary vars
  pipes = std::vector<Pipe>(numWorkers);
  results = std::vector<int>(numWorkers);

  // Print information
  printf("\
File: %s\n\
Lines: %d\n\
Lines per worker: %d\n\n", searchPath, lineCount, chunkSize);

  // Spawn child processes
  spawnProcesses();

  // Get final match count and display value
  int finalResult = reduce();
  printf("\nTotal occurences: %d\n", finalResult);

  return 0;
}

/**
 *
 * Display program usage
 */
void usage() {
  printf("\nUsage: ./project2 <num-workers> <keyword> <path>\n");
}

/**
 *
 * Map each line to its byte index
 *
 * @param  {char*}  path  Path to file
 * @return {int}          Total number of lines
 */
int mapLines(char* path) {
  std::ifstream in(path, std::ifstream::in);

  if (!in.is_open()) {
    printf("Failed to open file for reading!\n");

    return -1;
  }

  std::string line;

  int count = 0;

  while (std::getline(in, line)) {
    lineMap[count] = in.tellg();
    count++;
  }

  return count;
}

/**
 *
 * Fork a process for each worker
 */
void spawnProcesses() {
  for (int i = 0; i < numWorkers; i++) {
    spawnProcess(i);
  }
}

/**
 *
 * Create pipes, fork, and pass args to child via pipe.
 * Child process writes result to parent-bound pipe then exits.
 *
 * @param  {int}  index  Child process index/number
 * @return {int}         PID of new child process
 */
int spawnProcess(int index) {
  int* toChild = pipes[index].toChild;
  int* toParent = pipes[index].toParent;

  // Attempt to create the two pipes necessary for communication
  if (pipe(toChild) < 0 || pipe(toParent) < 0) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  // Attempt to fork child process
  int pid = fork();

  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  // If we are the child process, proceed with the appropriate logic and exit
  if (pid == 0) {
    childProcess(toChild, toParent);
    exit(0);
  }

  // Pass line information to child process via pipe
  int mapStart = (index * chunkSize);
  int mapLength = (index < numWorkers - 1 ? chunkSize : lastChunkSize);

  write(toChild[1], &index, sizeof(int));
  write(toChild[1], &mapStart, sizeof(int));
  write(toChild[1], &mapLength, sizeof(int));

  return pid;
}

/**
 *
 * Child process logic. Reads args from child-bound pipe, maps, then
 * passes result to parent process via parent-bound pipe.
 *
 * @param {int}  toChild   Child-bound pipe file descriptor
 * @param {int}  toParent  Parent-bound pipe file descriptor
 */
void childProcess(int* toChild, int* toParent) {
  int childIndex, mapStart, mapLength;

  // Read line information from parent via pipe. If read fails, exit.

  if (read(toChild[0], &childIndex, sizeof(childIndex)) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  if (read(toChild[0], &mapStart, sizeof(mapStart)) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  if (read(toChild[0], &mapLength, sizeof(mapLength)) < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  printf("Child %d spawned. Mapping from line %d to %d!\n", childIndex, mapStart, mapStart + mapLength - 1);

  // Get matches in file using map function
  int result = mapFunc(searchPath, searchKeyword, lineMap[mapStart], mapLength);

  // Write result to parent-bound pipe
  if (write(toParent[1], &result, sizeof(result)) < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }

  exit(0);
}

/**
 *
 * Find pattern matches line by line in a file starting at 'start' byte,
 * until 'length' lines have been read.
 *
 * @param  {std::string}  path       Path to file
 * @param  {std::string}  pattern    String pattern to match
 * @param  {int}          startByte  Byte to start from in file
 * @param  {int}          numLines   Number of lines to read
 * @return {int}                     Number of matches
 */
static int mapFunc(std::string path, std::string pattern, int startByte, int numLines) {
  std::string contents;

  // Open input file stream and skip to appropriate position in file
  std::ifstream in(path.c_str(), std::ifstream::in);
  in.seekg(startByte);

  // Ensure that the input stream was opened properly
  if (!in.is_open()) {
    printf("Failed to open file for reading!");
    exit(1);
  }

  std::string line;
  int lineCount = 0;

  // Read lines from file until numLines, appending each to 'contents', then
  // close the stream cleanly.
  while (lineCount < numLines) {
    std::getline(in, line);

    contents.append(line);

    lineCount++;
  }

  in.close();

  int matchCount = 0;
  std::string::size_type start = 0;

  // Calculate number of matches using contents read from file
  while ((start = contents.find(pattern, start)) != std::string::npos) {
    matchCount++;
    start += pattern.length();
  }

  return matchCount;
}

/**
 *
 * Collects results from child processes via parent-bound pipes, then sums
 * values to get the final number of matches. Function returns when all
 * child processes have exited.
 *
 * @return {int}  Total number of matches from child processes
 */
int reduce() {
  int result, finalResult = 0;
  int* toParent;

  // Iterate through workers and read result from each. Loop waits until the
  // result is ready on the pipe, then reads the value and closes the pipe.
  //
  // The final value is incremented by each result.
  for (int i = 0; i < numWorkers; i++) {
    toParent = pipes[i].toParent;

    if (read(toParent[0], &result, sizeof(result)) < 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    close(toParent[0]);
    close(toParent[1]);

    printf("Child %d found %d matches.\n", i, result);

    finalResult += result;
  }

  // Wait for all child processes to exit.
  while (wait(NULL) > 0) { }

  return finalResult;
}
