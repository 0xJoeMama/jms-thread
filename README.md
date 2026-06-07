# jms

A job management system for POSIX (now using threads and sockets for remote connections).

Brought to you by:
- sdi2499168, Petros Plastiras

# How to
## Building
To build this project, you need to make GNU Make, a C++ compiler and a libc implementation on your system.

To build using Make and the default C++ compiler on your system, run:
```sh
$ make
```

To build using Make and your preferred C++ compiler, run:
```sh
$ make CXX=<insert-compiler-name>
```

Any of the above commands will build the whole project. This includes the `jms_coord` and `jms_console` executables.

To remove all build artifacts (including executables), run:
```sh
$ make clean
```

## Running
To run the project you first need to start a `jms_coord` process by providing a port, a runtime path as well as the maximum
number of processes that can be active at any one time.

An example execution could be:
```sh
$ ./jms_coord -p 6767 -l run -n 12
```

The above will start a new coordinator with its runtime directory being `./run` and with a maximum of 12 jobs being run at a specific point in time.

Next, you can start up a `jms_console` instance, by providing the port and the IP or DNS name for the machine you started the coordinator on,
as well as an optional operations file to be read and streamed to the coordinator, line by line.

An example console execution would look like this:
```sh
$ ./jms_console -h localhost -p 6767 -o inputs/sleeps_short.input # assume coordinator is being run locally
```

If you provided an operations file, the console will send all the commands from the input file to the coordinator.
After that, the console will enter interactive mode, which will allow you to directly input commands into the system.

After running a command, the console will show you the result of executing that command on the coordinator.

# Design
The system is split up into 4 important subsystems:
1. The console
2. The coordinator
3. The worker pool
4. The client handlers

## The console
The console is probably the simplest part of the whole system. It is a tool that streams commands into the input of the coordinator and reads the responses from its output.

## The coordinator
The coordinator is the brain of the system. It's the one that stores all program state and handshakes clients into the whole system.
It holds a list of all issued jobs, their current state, their submission time and some more metadata that is used for when that job is scheduled.
The main job of the coordinator during the program's active phase, is to welcome new console connections into the system.

The main loop of the coordinator consists of waiting for client connections and dispatching them to a new client handler thread.

## The worker pool
The worker pool is a simple thread pool that contains threads that are either waiting for a job to finish
(having dispatched it to a new process) or for a job to become available. When executing a job it updates the metadata
on the coordinator to accurately reflect the job's state.

## The client handlers
The client handlers are detached threads with handles to the shared state created by the coordinator thread and updated by the worker pool.
Client handlers wait for string commands from clients, parse them, execute them and return responses to the clients.
Commands and responses are streamed as plain-text streams, prefixed with a 32-bit unsigned integer size (in binary format).
Care has been taken to properly translate between machine byte order and network byte order for integers.
A handler reads a client command (or a status update, like clients disconnecting) and reacts to it by changing the shared state.
For example, when a job submission is sent, a new job is added to the job list of the coordinator and a new job is appended at the end of the worker pool's job queue.
Then based on the changes it made, it returns information to the issuing client. Since these threads are detached when they exit,
they are cleaned up immediately, meaning the coordinator doesn't really have to do any cleanup on its side.

## Commands
A choice has been made for commands and responses to be simple strings. This allows for easier parsing and debugging.
For more insight look into [this file](./include/command.hpp).

## Specific Choices
### C++ Wrappers
Some C++ wrapper classes around basic pthread abstractions have been created, in order to allow for more easy language integration and cleaner code.

### Shutdown
When a shutdown is issued from a client, a signal is sent to the coordinator threads (SIGTERM in our case), in order to terminate waiting for clients.
This is required as the main thread is stuck in an accept call and needs to be notified when the shutting_down variable has been changed.

### SIGPIPE
The SIGPIPE posix signal has been blocked for the whole process in order to allow for closed connections to only kill the thread that was waiting on them.
That thread will get an EPIPE error whenever it tries to write to the closed socket which will throw a fatal exception for it, the way our program is structured.

