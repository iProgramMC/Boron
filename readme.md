# The Boron Operating System

Boron is a portable (32-bit and 64-bit) operating system designed with SMP in mind. It borrows heavily from Windows NT,
but does not aim to be a total clone.

### License

This project is licensed under the three clause BSD license, **except the following**:

- Flanterm: https://codeberg.org/Mintsuki/Flanterm.git - Licensed under the 2 clause BSD license
    - The `external/flanterm` submodule
    - `external/flanterm_alt_fb.c`
    - `external/flanterm_alt_fb.h`

- The implementation of splay and RB trees from FreeBSD - Licensed under the 2 clause BSD license
    - `common/include/rtl/fbsdtree.h`

### Why the codename Boron?
(Note: The name is probably temporary, not sure, but this is what it's called right now)

* It's short, with only two syllables
* It's new (as far as I can tell)
* The chemical element with the same name has the atomic number of 5. Coincidentally this is also my 5th
  operating system project. (three of them flopped, and the other one that didn't is [NanoShell](https://github.com/iProgramMC/NanoShellOS))

### Supported platforms

Currently, Boron supports running on:
- x86_64 (64-bit)
- x86 (32-bit)
- ARMv6 (32-bit)

Note: The supporting code for 32-bit x86 is called `i386`, but there is a long way to actually running on the original 80386.)

### Planned ports

Boron is planned to be ported to:
- PowerPC (32-bit)
- ARMv6 (32-bit)
- AArch64 (64-bit)

## Building

To set up the repo for building, run the following commands:
```
git submodule update --init --recursive
make -C limine
```

### AMD64

To build for amd64, you will need to use an x86_64 native distribution of `gcc`, `binutils`, `nasm`,
and then additionally, `tar` and `xorriso`.

AMD64 is the default platform, so you can simply run:
```
make
```

Or, if you want to be explicit,
```
make TARGET=AMD64
```

To run, invoke `qemu-system-x86_64` with `-cdrom build/image.amd64.iso -boot d`.  The run scripts
available in `tools` might not work.

### i386

To build for i386, you will need to use `clang-22`, `binutils`, `nasm`, and then additionally,
`tar` and `xorriso`.

Run:
```
make TARGET=I386
```

To run, invoke `qemu-system-i386` with `-cdrom build/image.i386.iso -boot d`. The available run
scripts in `tools` probably won't work.

### ARM

The ARM port currently targets legacy Apple devices such as the **iPhone 3G** and **1st gen iPod touch**.
As such, setting up this port is a little harder.

First, check out and compile [my fork of OpeniBoot](https://github.com/iProgramMC/openiBoot). This
is not a prerequisite to *build* the kernel, but you won't be able to run it in the original version
of OpeniBoot.

This will require an ancient Linux distro (e.g. Debian 8.7.0), since the [arm-elf gcc 4.x toolchain](https://github.com/iDroid-Project/OpeniBoot-toolchain)
won't build with modern compilers.  I have tried with the modern `arm-none-eabi-gcc` toolchain, but
it straight-up doesn't work.  It will also require `python2.7-dev` and `sconscript`.  To compile,
run `scons iPhone3G` or `scons iPodTouch1G`.

Then, get `arm-none-eabi-gcc`, `arm-none-eabi-ld`, and `tar`.

Finally, you should be able to compile using:
```
make TARGET=ARM
```

To run, put your iPod/iPhone in recovery mode, then `loadibec ipt_1g_openiboot.img3` from the OpeniBoot
dir (or `loadibec iphone_3g_openiboot.img3`).

Press the Home button on your device when it lets you select `Console`, and then type in `sudo oibc`
on your computer, with the device still connected. Afterwards, type in `!`, and then the path to
`build/image.arm.tar`. Finally, type `borongo`. Boron should boot on your iDevice. It's not even
minimally interactable on there, though.

#### Running In An Emulator

There is actually an emulator (admittedly, half-baked) for the iPod touch 1G.

First, check out and compile [my fork of devos50's qemu-ios](https://github.com/iProgramMC/qemu-ios).

Then you should be able to use the following command line to run Boron on it. (Note: you will still
need OpeniBoot to boot)
```
./qemu-system-arm \
	-M iPod-Touch,bootrom=[bootrom path],iboot=[openiboot raw image path],nand=[nand path],oib-elf=[path to build/image.arm.tar] \
	-serial stdio \
	-cpu max \
	-m 1G \
	-d unimp \
	-pflash [nor path]
```

For details on how you can get the bootrom, nand dump, and nor flash data, check out
[devos50's article on emulating the iPod touch](https://devos50.github.io/blog/2022/ipod-touch-qemu-pt2).

Note: as it is, the NAND image will not work, since it's in a bunch of folders.  You will need to
build a unified nand image by running `tools/ipt_1g_ungenerate_nand.cpp` with your nand folder and
it'll generate a `nand.img` for use in my fork.

## Goals/plans

#### Source code layout
Currently, the OS's source is structured into the following:

* `boron/` - The OS kernel itself. See [the Boron kernel's structure](boron/structure.md) for more details.

* `drivers/` - The actual drivers themselves.

* `user/` - Boron's native userspace distribution lives here.

Kernel DLL exports will use the prefix `OS`. I know this isn't the chemical symbol for boron (that being B),
but it is what it is.

#### Basic features

- Portable, layered design

- Supports SMP from birth

- Dynamically linked kernel modules (drivers)

- Hardware abstraction layer which allows for the same kernel executable to run on the same ISA across
  different platforms, loaded as a kernel module

- Nested interrupts thanks to an interrupt priority system

#### Subsystems
If an item is checked, that means it's being worked on or is complete. If not, that means that no code is at
all present related to it.

* [x] Kernel core (`Ke`)
	* [x] Spin locks
	* [x] IPLs (interrupt priority levels, analogous to NT's IRQL)
	* [x] Dispatcher (timers, mutexes, events, semaphores...)
	* [x] Scheduler
	* [x] DPCs (deferred procedure calls)
	* [x] Interrupt dispatching
	* [x] APCs
	* [x] User mode programs

* [x] Memory manager (`Mm`)
	* [x] Page frame database
	* [x] Kernel heap
	* [x] Reclamation of pages occupied by initialization code
	* [x] Capture of physical regions for extended use
	* [x] Mapping and unmapping anonymous memory
	* [x] File backed memory
	* [ ] Swap file support
	* [ ] Swap out page tables
	* [ ] Swap out kernel code

* [x] Object manager (`Ob`)
	* [x] Object creation
	* [x] Object deletion
	* [x] Object lookup
	* [x] Deferred object deletion
	* [x] Symbolic link support

* [x] I/O manager (`Io`)
	* [x] Device objects
	* [ ] System services
	* [x] File objects
	* [ ] File system/volume objects
	* [ ] ...

* [x] Dynamic linked library loader (`Ldr`)
	* [x] HAL separate from kernel
	* [x] Load additional kernel side modules
	* [x] Map the Boron system service DLL (libboron.so)

* [ ] File system manager
	* [ ] ...

* [ ] Cache manager (later)
	* [ ] ...

* [ ] Security subsystem (later)
	* [ ] ...

* [x] Boron DLL / Librarian / System service handler
	* [x] Load fixed modules
	* [x] Load relocatable modules
	* [x] Link modules with each other by resolving undefined dependencies

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
