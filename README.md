# Lock-Free Datastructures in C

A collection of lock-free data structures and tests that are written in C99 and
compiled with GCC. The goal is to ensure that all of these data structures are thread
safe and scale as efficiently as possible when run in parallel. The code is also
intended to be a learning tool for anyone exploring how to use [atomic operations in C](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html).
See the summary of how it works.


# Performance Tests

TODO: show performance scaling with multiple threads


# Thread-safe, Atomic Lock-Free Operations

TODO: better describe how CAS works and the core tactics used here.

TODO: Linked List insert via CAS

TOOD: Linked List delete via CAS


# Tests

Tests are in the `test` sub-directory. Each test has a `make_test_....sh` script 
that'll compile and test the code. See the tests for example usage.


# Data Structures

TODO: a short list of all datastructures available here.
