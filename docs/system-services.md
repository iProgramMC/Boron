# Boron - System service documentation

This document serves to document the system services provided by the Boron operating system, which
are classified into three different types:

- Kernel system services: These are generally *mirrored* in `libboron.so`, but are implemented in
  the operating system kernel. These system services generally implement features that cannot be
  implemented purely in user-mode code, because they require additional privileges. For example,
  `OSSleep` cannot be implemented purely in user-mode code, because a time source is necessary,
  and that is not typically available in user-mode without a system service.

- User system services: These are generally *implemented* in `libboron.so`, but may issue kernel
  system service calls.  These include wrappers around system services, such as `OSPrintf` wrapping
  `OSWriteFile`.

- User non-system services: These are not implemented in either the operating system kernel or
  `libboron.so`, but instead, dedicated libraries such as `libCoreGraphics.so`. These will not be
  described in this document.

**This document is currently a work in progress, so it does not yet reflect the complete state of
the project and should be taken with scrutiny.**

## Kernel System Services

Kernel system services generally follow a set of conventions that will be implicit from now on:

- A kernel system service will typically return `STATUS_SUCCESS` if the entire operation succeeded.
    - Except for I/O services, which may return `STATUS_BLOCKING_OPERATION` or `STATUS_END_OF_FILE`.
      These are also considered successful codes, but by default, `SUCCEEDED(Status)` only checks
      for equality with `STATUS_SUCCESS`. You must use the `IOSUCCEEDED()` or `IOFAILED()` macros
      when handling status codes from I/O system services.

- `STATUS_INVALID_PARAMETER` will be returned if invalid parameters have been passed.  For example,
  passing addresses belonging to the "higher half" of the address space (or non-canonical addresses
  on some architectures), size values that overflow into the higher half, or cover so much of the
  address space that the ending address is smaller than the beginning address.

- `STATUS_FAULT` will be returned if an access to a memory region fails.  For some system calls,
  the operation of the system service was still committed, but the results could not be transmitted
  back to userspace.

### 1. `OSAllocateVirtualMemory`

Allocates a region of virtual memory of a certain size, specified in `RegionSizeInOut`, optionally
fixed starting at `BaseAddressInOut`, and optionally overriding any existing allocations, with
specified protection levels.  The newly allocated memory can be committed or simply reserved, to be
committed or released later.  The allocation may be applied in the current process, or in another
process, a handle for which the current process owns.

```c
BSTATUS OSAllocateVirtualMemory(
	HANDLE ProcessHandle,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
);
```

Regions can be reserved by specifying `MEM_RESERVE` for the `AllocationType`, and committed by
specifying `MEM_COMMIT`.

#### Parameters

`ProcessHandle`

The process handle to modify.  Generally this should be `CURRENT_PROCESS_HANDLE`, unless the current
process is responsible for creating other processes.

`BaseAddressInOut`

The base address of the virtual memory region to allocate.

`RegionSizeInOut`

The size of the virtual memory region to allocate.

`AllocationType`

The type of allocation performed.  This is a combination of the flags `MEM_COMMIT`, `MEM_RESERVE`,
`MEM_FIXED`, `MEM_TOP_DOWN`, `MEM_SHARED`, and `MEM_OVERRIDE`.

`Protection`

The requested protection for the address range.  This is a bit combination of `PAGE_READ`,
`PAGE_WRITE` and/or `PAGE_EXECUTE`.

#### Return value

This can return one of the following status codes:

- `STATUS_INSUFFICIENT_MEMORY`: The system does not have enough memory to fulfill the request.  This
  could be because too much memory was allocated, or because the entire available virtual memory
  space was previously reserved.

- `STATUS_TYPE_MISMATCH`: If the `ProcessHandle` handle is not actually a handle to a process, or
  `CURRENT_PROCESS_HANDLE`.

#### Remarks

Memory created through this function is automatically initialized to zero.

Some protection values may not function correctly on all platforms. For example, the `PAGE_READ`
protection bit cannot be turned off on i386 and AMD64 platforms.

If `MEM_FIXED` is set, or `MEM_COMMIT` is set without `MEM_RESERVE`, then the contents of
`BaseAddressInOut` are read and used as the base address of the commit.  Otherwise, they are
ignored.

When committing an already reserved region with `OSAllocateVirtualMemory`, `Protection` may be
set to zero. In this case, the protection value of the committed pages will simply be the same
as the protection value provided when initially reserving the page. In all other cases, `Protection`
should not be set to zero.

Specifying `MEM_FIXED` with `*BaseAddressInOut == NULL` will simply behave as if `MEM_FIXED` was not
set. Regions overlapping the first page of user-accessible address space are invalid and cannot be
created.

If `MEM_OVERRIDE` was specified, then `MEM_FIXED` must be set.
