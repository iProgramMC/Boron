# The Boron Operating System

Boron is a 64-bit operating system designed with SMP in mind. It borrows heavily from Windows NT,
but does not aim to be a total clone.

Note! Boron is currently not even in a minimally usable state. Don't expect it to do anything.

This project is licensed under the three clause BSD license, **except the following**:
- Flanterm (drivers/halx86/source/flanterm): https://github.com/mintsuki/flanterm - Licensed under the 2 clause BSD license
- The implementation of splay and RB trees from FreeBSD (boron/include/rtl/fbsdtree.h) - Licensed under the 2 clause BSD license

#### Be advised that this is currently alpha-level software and you should not expect any stability from it.

Currently this uses Limine v7.x.

### Why the codename Boron?
(Note: The name is probably temporary, not sure, but this is what it's called right now)

* It's short, with only two syllables
* It's new (as far as I can tell)
* The chemical element with the same name has the atomic number of 5. Coincidentally this is also my 5th
  operating system project. (three of them flopped, and the other one that didn't is [NanoShell](https://github.com/iProgramMC/NanoShellOS))

## Building
In a terminal, run the following commands:
```
git submodule update      # downloads limine and flanterm
make -C limine
make
```
(note: these are to be done on Linux or WSL. Cygwin/MinGW32 were not tested and don't work due to the differences in executable formats.)

To run, invoke `./run-unix.sh` or `make run`. If you are using WSL 1, you can do `./run.sh`
to run the built iso using your native QEMU installation on Windows.

## Goals/plans

#### Source code layout
Currently, the OS's source is structured into the following:

* `boron/` - The OS kernel itself. See [the Boron kernel's structure](boron/structure.md) for more details.

* `drivers/` - The actual drivers themselves.

Kernel DLL exports will use the prefix `Brn`. I know this isn't the chemical symbol for boron (that being B),
but it is what it is.

#### Basic features

- Portable, layered design

- Supports SMP from birth

- Dynamically linked kernel modules (drivers)

- Hardware abstraction layer which allows for the same kernel to run
  across different ISAs, loaded as a kernel module

- Nested interrupts thanks to IPLs

#### Subsystems
If an item is checked, that means it's being worked on or is complete. If not, that means that no code is at
all present related to it.

* [x] Kernel core
	* [x] Spin locks
	* [x] IPLs (interrupt priority levels, analogous to NT's IRQL)
	* [x] Dispatcher (timers, mutexes, events, semaphores...)
	* [x] Scheduler
	* [x] DPCs (deferred procedure calls)
	* [x] Interrupt dispatching
	* [x] APCs
	* [ ] User mode programs

* [x] Memory manager
	* [x] Page frame database
	* [x] Mapping and unmapping anonymous memory
	* [ ] File backed memory
	* [ ] Swap file support
	* [ ] Swap out page tables
	* [ ] Swap out kernel code

* [x] Object manager
	* [x] Object creation
	* [x] Object deletion
	* [x] Object lookup

* [x] I/O manager
	* [ ] File objects
	* [ ] File system/volume objects
	* [ ] ...

* [ ] File system manager
	* [ ] ...

* [ ] Cache manager (later)
	* [ ] ...

* [ ] Security subsystem (later)
	* [ ] ...

* [ ] User space
	* [ ] Command line shell
	* [ ] Test programs
	* [ ] ...
	* [ ] Window manager
	* [ ] ...
	* [ ] Running NanoShell applications natively???
	* [ ] ...

* [ ] I/O manager improvements
	* [ ] Asynchronous I/O support...

* [ ] ...

A lot of this is still work in progress and I have yet to figure out a bunch of stuff, so wish me luck :)
