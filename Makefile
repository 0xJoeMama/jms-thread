CXXFLAGS =-O3 -Wall -Werror -Wextra -std=c++17 -pedantic -Iinclude -g3 $(EXTRAFLAGS) -pthread
LDLIBS = -lpthread -lstdc++
LDFLAGS=-pthread
CXX ?=gcc
BINS = jms_coord
TESTS=thread_tests

vpath %.cpp src/
vpath %.cpp tests/
vpath %.hpp include/

all: $(BINS)

jms_coord: jms_coord.o

test: $(TESTS)

clean:
	rm -rf $(BINS) *.o
