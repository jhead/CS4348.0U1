##
## CS4348 - Project 2 Makefile
## Justin Head (jxh122430@utdallas.edu)
##

## C compiler configuration
CXX = g++
CXXFLAGS = -Wall

## General configuration
EXEC = project2
OBJ = $(EXEC).o

## Default target
all: clean $(EXEC)

$(EXEC):
	$(CXX) $(CXXFLAGS) -o $@ $@.cc -lm

## Clean target
clean:
	rm -f *.o $(EXEC)
