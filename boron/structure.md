## The Project Structure of the BORON kernel

Currently this is only a plan.

* `ke/`: Architecture independent 'sub-kernel'-module. Implements SMP, locking,
  and the scheduler.
  
* `mm/`: Memory management module - handles page faults, copy-on-write, demand
  paging, maybe even compression and swapping in the future
  
* `ha/`: Hardware abstraction layer. Abstractions provided for each hardware
  platform that Boron will compile on, such as the PC. Implements timer support,
  among other things
  
* `arch/`: Architecture abstractions. These abstract the CPU away from other
  parts of the kernel. It handles page table management, context switching,
  interrupt priority level setting, and CPU intrinsics

* `rtl/` - Run-time library - Includes functions such as sprint, memcpy, strcpy.