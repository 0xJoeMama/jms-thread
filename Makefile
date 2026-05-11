CXXFLAGS =-O3 -Wall -Werror -Wextra -std=c++17 -pedantic -Iinclude $(EXTRAFLAGS)
CXX ?=gcc
BINS = jms_coord

vpath %.cpp src/
vpath %.hpp include/

all: $(BINS)

clean:
	rm -rf $(BINS) *.o
