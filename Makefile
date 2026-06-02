CXXFLAGS =-O3 -Wall -Werror -Wextra -std=c++17 -pedantic -Iinclude -g3 $(EXTRAFLAGS) -pthread -MMD -MD
LDLIBS = -lpthread -lstdc++
LDFLAGS=-pthread
CXX ?=gcc
BINS = jms_coord jms_console
TESTS=thread_tests thread_pool_tests

vpath %.cpp src/
vpath %.cpp tests/
vpath %.hpp include/

all: $(BINS)

jms_coord: jms_coord.o thread_pool.o coord_commands.o job.o file_store.o
jms_console: jms_console.o
thread_pool_tests: thread_pool.o
test: $(TESTS)

clean:
	rm -rf $(BINS) $(TESTS) *.o *.d

-include *.d
