# The Boron Operating System

#### EXPERIMENTAL EXPERIMENTAL EXPERIMENTAL EXPERIMENTAL EXPERIMENTAL

Boron is a 64-bit operating system designed with SMP in mind.

The project is wholly licensed under the three clause BSD license, **except the following**:
- Flanterm (source/ha/flanterm): https://github.com/mintsuki/flanterm - Licensed under the two clause BSD license

#### Be advised that this is currently alpha-level software and you should not expect any stability from it.

Currently this uses Limine v4.x. An upgrade is planned.

### Why the name Boron?

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

Kernel DLL exports will use the prefix `Bn`. I know this isn't the chemical symbol for boron (that being B),
but it is what it is.

#### Architecture design
There's hardly a decided architecture design, as this is right now at the experimental stage.
Here are some ideas. Think of it like a scribble board of randon junk:
* Worker thread centered design. This can also be interpreted as a clients-server architecture.
* Each CPU has its own kernel heap. This reduces TLB shootdowns. If there's a need to transfer
  data between CPUs, one may use an IPI with a list of physical pages.
* Until swapping to disk is added, use a form of poor man's compression - if a page is filled
  to the brim with a single byte that fills unspecified criteria, it'll be "compressed" down
  into a single page entry.

#### Primordial tasks
* [x] Hello World
* [x] Physical memory manager
* [x] Safe locking
* [x] SMP Bootstrap
* [ ] Inter-processor communication (through IPIs)
* [ ] Task switching and concurrency
* [ ] Virtual memory manager
* [ ] Inter-process communication

#### Other features
* [ ] Init ram disk file system
* [ ] Ext2 file system support
* [ ] More... (still not decided)

#### Drivers
* [ ] Limine terminal
* [ ] PS/2 Keyboard
* [ ] Own terminal with framebuffer
* [ ] Serial port
* [ ] PCI
* [ ] PS/2 mouse

#### User
* [ ] A hello world
* [ ] A stable API
* [ ] A basic shell
Still to be decided.

#### Far, far in the future
* [ ] Compatibility with [NanoShell](https://github.com/iProgramMC/NanoShellOS)?
* [ ] Networking?
* [ ] USB (could also backport to NanoShell32 itself)

