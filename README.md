# Lock-Free Datastructures in C

A collection of lock-free data structures and tests that are written in C99 and
compiled with GCC. The goal is to ensure that all of these data structures are thread
safe and scale as efficiently as possible when run in parallel. The code is also
intended to be a learning tool for anyone exploring how to use [atomic operations in C](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html).
See the summary of how it works.


# Performance Tests

TODO: show performance scaling with multiple threads


# Thread-safe, Atomic Lock-Free Operations

If multiple threads access the same variables directly or through data structures, synchronization is usually required. Often a "lock" is somehow enforced in user space so that only one thread at a time will access the shared memory location. The core thing this code does is avoid a user-space lock in favor of letting the computer's hardware lock the memory, usually with at atomic compare-and-swap (CAS) style assembly instruction. Typically hardware locks are much, much faster than user-space software-based locks. Here is a [great blog article](https://attractivechaos.wordpress.com/2011/10/06/multi-threaded-programming-efficiency-of-locking/) to read with different synchronization approaches in C and relative performance.

## Example Problem

Consider a simple example of the multiple thread, shared memory problem: incrementing a number.

```
int count = 0;

void increment() {
  count += 1;
}
```

Depending on how it is compiled, this likley does the following.

1. Read the value from count
2. Store the value of count + 1 back to count

Now with multiple threads, it may work fine like the following:

1. Thread 1: reads the value of `count` as 0 (we'll call it `count1`)
2. Thread 1: save the value of `count1` + 1 back to `count`. Now `count` = 0 + 1 = 1 
3. Thread 2: reads the value of `count` as 1 (we'll call it `count2`)
4. Thread 2: save the value of `count2` + 1 back to `count`. Now `count` = 1 + 1 = 2

Or you may have this problem if operations don't go in the ideal order.

1. Thread 1: reads the value of `count` as 0 (we'll call it `count1`)
2. Thread 2: reads the value of `count` as 0 (we'll call it `count2`)
3. Thread 1: save the value of `count1` + 1 back to `count`. Now `count` = 0 + 1 = 1 
4. Thread 2: save the value of `count2` + 1 back to `count`. Now `count` = 0 + 1 = 1

The final value of `count` is problematically off by 1. It is common to see this in multi-threaded code when stats are being tracked and no synchronization is being used to make sure counts increment as expected.

# Example Solutions

Depending on the language there are a few solutions. 

1. Only use one thread. Sure, you are stuck with the performance of one CPU but you don't have to worry about this.
   * Python does this. The Global Interpreter Lock (GIL) is awesome until your code's performance isn't enough
   * JavaScript does this. Code sets up callbacks that get executed by a single thread in the background.
   * Most languages do this by default unless you purposely start another thread
2. User-space code or data locking
   * Isolave what code runs. C, C++, Java and most languages capable of parallel processing have this. For example, the `pthread` library in C lets you setup mutual exclusion (aka mutex) locks that you can lock and unlock so that only one chunk of code runs at a time, presumably variables that must reliably be updated.
   * Isolate what data is used. Rust's channels are a good example of this. Ownership of the data is passed around, ensuring only one thread modifies it at a time.
3. Hardware-based memory locking
   * Atomic compare-and-swap (CAS) style is probably best known. Your computer's hardware can enfore single threaded access to memory locations. For example see AMD64's `LOCK` keyword can be used on several operations, including the `ADD` opcode. 
   * Many languges with parallel processing wrap CAS-operations with convenient user-space methods and data structres. For example, Java exposes them with atomic variables and C/C++ have atomic keywords too.

It isn't that CAS-style operations are best, but they are typically very fast for general purpose use. They are also what this repo is focusing on. That is why they get all the attention here.
 
## Example Code
The [AMD64 ArchitectureProgrammer’s Manual](https://www.amd.com/system/files/TechDocs/24594.pdf) section 1.2.5 notes the key stuff we're using here: `LOCK`.

> 1.2.5    Lock Prefix
>
> The LOCK prefix causes certain kinds of memory read-modify-write instructions to occur atomically.The mechanism for doing so is implementation-dependent (for example, the mechanism may involvebus signaling or packet messaging between the processor and a memory controller). The prefix isintended to give the processor exclusive use of shared memory in a multiprocessor system.
>
> The LOCK prefix can only be used with forms of the following instructions that write a memoryoperand: ADC, ADD, AND, BTC, BTR, BTS, CMPXCHG, CMPXCHG8B, CMPXCHG16B, DEC,INC, NEG, NOT, OR, SBB, SUB, XADD, XCHG, and XOR. An invalid-opcode exception occurs if the LOCK prefix is used with any other instruction. 

This locking is important because it'll mean the operation either happens fully or not. Consider the docs for `CMPXCHG` (aka Compare Exchange). Ignore the specific register names.

> Compares the value in the AL, AX, EAX, or RAX register with the value in a register or a memorylocation (first operand). If the two values are equal, the instruction copies the value in the secondoperand to the first operand and sets the ZF flag in the rFLAGS register to 1. Otherwise, it copies thevalue in the first operand to the AL, AX, EAX, or RAX register and clears the ZF flag to 0.The OF, SF, AF, PF, and CF flags are set to reflect the results of the compare.When the first operand is a memory operand, CMPXCHG always does a read-modify-write on thememory operand. If the compared operands were unequal, CMPXCHG writes the same value to thememory operand that was read.

Note that the above says there is a conditional copy and a flag that indicates if the copy was done or not. There are also various forms of `CMPXCHG`-like operands for different sized data. You might be surprised to find out that `LOCK CMPXCHG` is the main magic that makes this very fast, hardware level synchronization work.

Here is the solution in C-like pseduo code (note that `__atomic_cmpxchg` isn't real it is a simplifaction of the C++11 based [atomic built-ins](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html) that libc offers).

```
int count = 0;

void increment(void) {
  int current_val = count;
  int desired_val = existing_val + 1;
  
  // try setting `count` to be `desired_val` but only if `count` is still the number we expected 
  if (!__atomic_cmpxchg(&count, current_val, desired_val) {
  
    // If the attempt to increment failed, it means some other thread(s) changed
    // `count` so that it no longer is equal to `c1`
    // Try again with a fresh read of whatever `count`'s value is now.
    increment();
  }
}
```

### C99 Atomic CAS Example

Here is a full C99 example. It almost matches above but you'll have to read the atomic built-in docs `__atomic_compare_exchange`'s arguments and also a main method is added below.

```
#include <stdbool.h>

int count = 0;

void increment(void) {

	int expected = count;
  int desired = expected + 1;

	bool success = __atomic_compare_exchange(&count, &expected, &desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	if (!success) {
		increment();
	}
}


void * main(void *args) {

	// increment a counter in a thread-safe fashion
	increment();
}
```


Assembly output can be obtained by running `gcc -S -o example_cas.s -c -std=gnu99 example_cas.c`

Notice this increment ends up using `LOCK CMPXCHGL`.

```
increment:
.LFB0:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        subq    $32, %rsp
        movq    %fs:40, %rax
        movq    %rax, -8(%rbp)

        xorl    %eax, %eax
        movl    count(%rip), %eax
        movl    %eax, -16(%rbp)
        movl    -16(%rbp), %eax
        addl    $1, %eax
        movl    %eax, -12(%rbp)
        movl    -12(%rbp), %eax
        movl    %eax, %ecx
        leaq    -16(%rbp), %rdx
        movl    (%rdx), %eax
        lock cmpxchgl   %ecx, count(%rip)      <------ Thread-safe attempt at the increment!
        movl    %eax, %ecx
        sete    %al 
        testb   %al, %al 
        jne     .L2 
        movl    %ecx, (%rdx)
.L2:
        movb    %al, -17(%rbp)
        movzbl  -17(%rbp), %eax
        xorl    $1, %eax
        testb   %al, %al 
        je      .L5 
        call    increment
.L5:
        nop
        movq    -8(%rbp), %rax
        xorq    %fs:40, %rax
        je      .L4 
        call    __stack_chk_fail
.L4:
        leave
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE0:
        .size   increment, .-increment
        .globl  main
        .type   main, @function

```

### C99 Mutex-based Example

This example is also in C99 but uses a very common method of sychronizing code so variables are accessed by one thread at a time. It uses the `pthread` library an a mutex. While the lock is kept on the mutex other threads can't obtain it. Instead they'll be put in a wait state and later restarted when the mutex is unlocked.

Note that `pthread` implementation can vary by platform. On Linux (as of writing) [`pthread_mutex_lock`](
https://linux.die.net/man/3/pthread_mutex_lock) uses a Fast User-Space Mutex (aka [futex](https://linux.die.net/man/2/futex)). It'll try an atomic CAS-style check to see if it is the only thread doing work, if so it continues. If not, the Linux kernel puts the thread to in a wait state and will later let it try again.

```
#include <stdbool.h>
#include <pthread.h>

int count = 0;

void increment(void) {
	
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	
	pthread_mutex_lock(&lock);

	count++;

	pthread_mutex_unlock(&lock);
}


void * main(void *args) {

	// increment a counter in a thread-safe fashion
	increment();
}

```

Assembly output can be obtained by running `gcc -S -o example_mutex.s -c -std=gnu99 example_mutex.c`. 

Notice this time that it is not as easy as pointing out one assembly instruction. You have to trace through `pthread` code, which ends up being a lot more logic and software-based work in cases of thread contention.

```
increment:
.LFB2:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        movl    $lock.2609, %edi
        call    pthread_mutex_lock
        movl    count(%rip), %eax
        addl    $1, %eax
        movl    %eax, count(%rip)
        movl    $lock.2609, %edi
        call    pthread_mutex_unlock         <----- Try to acquire the lock. Likely more than one CMPXCHG!
        nop
        popq    %rbp
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE2:
        .size   increment, .-increment
        .globl  main
        .type   main, @function

```

## Conclusion: "Lock-Free"

Knowing about the CAS-style assembly instrucitons is the main magic that makes this fast "lock-free" implementation work. "lock-free" is in quotes because technically hardware-based memory locking is being used. Locking is happening. It really means that user-space locking is largely avoided and that typically makes it fast.

Be careful when assuming that this CAS-style approach is always better. It generally is a safe assumption that it'll work well; however, most algorithms that use it are similar to the example `increment()`. Try CAS, if fail, try again. This can waste a lot of CPU cycles if a large number of threads are in contention to modify the same variable. It can also be more complex to create a data structure that works easily with CAS operations. User-space locking of code regions is arguably more familiar and intuitive.

Hopefully, this repo matures enough that don't have to fret over complexity of CAS-style thread safety. Just use its data structures!


# Tests

Tests are in the `test` sub-directory. Each test has a `make_test_....sh` script 
that'll compile and test the code. See the tests for example usage.


# Data Structures

TODO: a short list of all datastructures available here.
