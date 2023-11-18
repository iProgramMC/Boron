## The Boron Operating System - Style Guide

### Definitions

* Pascal case - Each word of a function's name starts with a capital letter. There is no separation between words. <br>
  Examples: `SystemStartup`, `DebugPrint`

* Snake case  - Each word of a function's name starts with a small letter. Words are separated by an underscore. <br>
  Examples: `system_startup`, `debug_print`

* Capital snake case - Same as regular snake case except that all letters are capital. <br>
  Examples: `SYSTEM_STARTUP`, `DEBUG_PRINT`

* Camel case - Same as Pascal case except that the first letter is small. <br>
  Examples: `systemStartup`, `debugPrint`

### Restrictions

* Lines shouldn't stretch for more than 125 characters. This is a soft limit that's usually respected.

* File names are all lowercase, and abide by the 8.3 restriction. (8 characters for the file name and 3 characters for
  the extension)

### Alignment

* When aligning, make sure to use spaces instead of tabs.

Correct:
```c
DbgPrint("Some values: %d %d %d",
         Value1,
         Value2,
         Value3);
```
Incorrect (and it'll be obvious why if you aren't using a tab width of 4):
```c
DbgPrint("Some values: %d %d %d",
		 Value1,
		 Value2,
		 Value3);
```

These two statements will look the same only if you prefer a tab width of 4.

### Function names

* Function names are written in Pascal case.

* All function names, except for static functions, must be preceded by a namespace. See [Namespaces](#namespaces) <br>
  Example: `KiSystemStartup`, `HalInitAcpi`

* Acronyms are written the same way as regular words. However, writing the acronym in capitals is also accepted. <br>
  Right: `Acpi`, OK: `ACPI`

### Function definitions and calls

Function definitions usually look the following way:
```c
return_type MyFunction(arg1, arg2, ...);
```

However, sometimes there is not enough space on one line. In that case, split the function declaration as follows:
```c
return_type MyFunction(
	arg1,
	arg2,
	arg3);```
where the opening parenthesis is on the line containing the function name, but the closing one is on the line containing
the last argument.

Don't do this - this isn't how we roll:
```c
return_type
MyFunction(
	arg1,
	arg2,
	arg3
);
```

And especially don't split the declaration if it isn't warranted. So something like this is totally OK: ```c
void KeWaitForMultipleObjects(
	int Count,
	void* Objects[],
	int WaitType,
	int WaitReason,
	int WaitMode,
	bool Alertable,
	PKWAIT_BLOCK WaitBlockArray)
```

But something like this isn't...:
```c
void DbgPrint(
	const char* Message,
	...);
```
... because we can write it on a single line without going over the maximum line length.

### Local variable names

Local variable names are written in Pascal case.

### Function argument names

These follow the same pattern as [Local Variable Names](#local-variable-names) do.

### Type names

* All structures must be typedef'd (except for Limine bootloader request structures). <br>
  Here is an example of a correct way to define a structure:
  
  ```c
  typedef struct MY_STRUCT_tag
  {
      //...
  }
  MY_STRUCT, *PMY_STRUCT;
  ```

* Type names are written in capital snake case.

* If part of the Ke namespace, `K` will be appended to the start of the type name, as in, `KPRCB`

* If part of the Mm namespace, `MM` will be appended to the start of the type name, as in, `MMPTE`

* An additional pointer typedef must be defined, with `P` appended to the start of the type name.

### Enumeration values

* All enumeration values are written in capital snake case. <br>
  Example:
  
  ```c
  enum
  {
      VALUE_ONE,
	  VALUE_TWO,
	  VALUE_SEVERAL,
  };
  ```

* If a type definition for an enum is needed, follow the [Type names](#type-names) rule.

### Macros

* All macro names are written in capital snake case.

* Parameter names follow the [Function arguments](#function-argument-names) rule.

### Namespaces

The Boron kernel is currently structured in the following namespaces:

* `Ke` - Kernel core and architecture specifics  (ex: `KeGetCurrentPRCB`)
* `Mm` - Memory manager (ex: `MmAllocatePhysicalPage`)
* `Hal` - Hardware abstraction layer (currently baked into the kernel) (ex: `HalMPInit`)

[Soon]
* `Ex` - Executive services (ex: `ExAllocatePool`)
* `Io` - I/O manager (ex: `IoAllocateIrp`)
* `Bn` - System calls/native interfaces (ex: `BnCreateFile`)

Sometimes one doesn't need to expose a function to the rest of the system. In that case, the prefix should be mutated to
mark this.

* `p` is added after the prefix if the function is private/static (ex. `MmpPlugLeaks`)
* `i` replaces the last letter or is added after the prefix if the function is internal but may be needed by other parts
  of the kernel (ex. `MiAllocateSlab`)

### Comments

All\* source code files will have the following comment header attached:
```
/***
	The Boron Operating System
	Copyright (C) <Year(s) of Maintenance> <Author or The Boron Operating System Contributors>

Module name:
	<Your File Name>
	
Abstract:
	<Your Module's Abstract>
	
Author:
	<Author Name Here> - <Creation Date - ex. 27 September 2023>
***/
```

Functions may (soon, must) have a standardized description that looks something like this:
```
/***
	Function description:
		Your Function's Description
	
	Arguments:
		ExampleArgument - Supplies an example argument
	
	Return Value:
		Your Return Value.
	
	[Optional] IPL:
		[Example] IPL <= IPL_DPC.
	
	[Optional] Resources held:
		[Example] KepExampleLock held for entire duration.
***/
```
