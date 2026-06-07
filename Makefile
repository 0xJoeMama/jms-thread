CXXFLAGS =-O3 -Wall -Werror -Wextra -std=c++17 -pedantic -Iinclude $(EXTRAFLAGS) -pthread -MMD -MD -DNDEBUG
LDLIBS = -lpthread -lstdc++
LDFLAGS=-pthread
CXX ?=gcc
BINS = jms_coord jms_console
TESTS=thread_tests thread_pool_tests highest_fd

vpath %.cpp src/
vpath %.cpp tests/
vpath %.hpp include/

all: $(BINS)
debug: CXXFLAGS += -UNDEBUG -g3
debug: all
test: $(TESTS)

.PHONY: clean all debug test

jms_coord: jms_coord.o thread_pool.o coord_commands.o job.o file_store.o
jms_console: jms_console.o
thread_pool_tests: thread_pool.o

clean:
	rm -rf $(BINS) $(TESTS) *.o *.d

-include *.d
