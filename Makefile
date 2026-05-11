CXXFLAGS =-O3 -Wall -Werror -Wextra -std=c++17 -pedantic -Iinclude -g3 $(EXTRAFLAGS)
CXX ?=gcc
BINS = jms_coord
TESTS=thread_tests

vpath %.cpp src/
vpath %.cpp tests/
vpath %.hpp include/

all: $(BINS)

test: $(TESTS)

clean:
	rm -rf $(BINS) *.o
