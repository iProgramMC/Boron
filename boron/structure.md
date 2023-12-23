## The Project Structure of The BORON Kernel

Currently this is only a plan.

* `ke/`: Architecture independent 'sub-kernel'-module. Implements SMP, locking,
  and the scheduler.

* `ex/`: Contains the executive initialization routines, as well as extra helpers
  that depend on kernel functions.
  
* `mm/`: Memory management module - handles page faults, copy-on-write, demand
  paging, maybe even compression and swapping in the future
  
* `arch/`: Architecture abstractions. These abstract the CPU away from other
  parts of the kernel. It handles page table management, context switching,
  interrupt priority level setting, and CPU intrinsics

* `rtl/` - Run-time library - Includes functions such as sprint, memcpy, strcpy.

* `ldr/` - DLL driver loader. Loads the driver DLLs into the kernel's memory region.

### Layering Diagram

Here is a mockup of the layering in Boron (still a WIP):

```
+-------------------------------------------------+
| User Mode                                       |
| +---------+ +---------+ +---------+ +---------+ |
| |   App   | |   App   | |   App   | |   App   | |
| +---------+ +---------+ +---------+ +---------+ |
+-----------------------||------------------------+
========================||========================= USER ^ / KERNEL v
+-----------------------||------------------------+
| System Call Handler                             |
+-----------------------||------------------------+
| Executive                                       |
|                                                 |
| +--------+  +--------+  +--------+              |
| |   Io   |  |   Cc   |  |  Mm 2  |              |
| +--------+  +--------+  +--------+              |
| +--------+  +--------+  +--------+              |
| |   Ob   |  |   Ps   |  |   Cm   |              |
| +--------+  +--------+  +--------+              |
+-----------------------||------------------------+
| Kernel Core                                     |
|                                                 |
| +--------+  +--------+  +--------+  +--------+  |
| |   Ke   |  |  Arch  |  |  Mm 1  |  |   Ldr  |  |
| +--------+  +--------+  +--------+  +--------+  |
+-----------------------||------------------------+
|                    Hardware                     |
+-------------------------------------------------+
```


