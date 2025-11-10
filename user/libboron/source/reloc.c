// Boron DLL
#include <boron.h>
#include <elf.h>

#ifdef __clang__

// Clang workaround:
//
// There is a bug where using any optimization level causes a bad offset
// to be added to the global offset table during dereference, causing a
// bad access while calling this function.  This is the mitigation.
//
// Example of disassembly from a Clang-compiled libboron.so without this
// workaround:
//
// RtlGetImageBase:
//     ; build stack frame
//     push ebp
//     mov  ebp, esp
//
//     ; determine current EIP
//     call _stuff
// _stuff:
//     pop  ecx
//
//     ; add the offset of the global offset table compared to said EIP
//     add  ecx, (offset _GLOBAL_OFFSET_TABLE_ - offset _stuff)
//
//     ; into eax, load the address of _DYNAMIC
//     lea  eax, (_DYNAMIC - _GLOBAL_OFFSET_TABLE_)[ecx]
//
//     ; buggy access. really not sure what's going on here
//     ; this access seems random but it always ends with 0xB so probably
//     ; *some* rhyme or reason even though it makes no sense for this to
//     ; exist at all
//     sub  eax, [ecx + 0x7CFB]
//
//     ; leave the function and return
//     pop  ebp
//     retn

#define CLANG_WORKAROUND __attribute__((optnone))

#endif // __clang__

#ifndef CLANG_WORKAROUND
#define CLANG_WORKAROUND
#endif

#define BUG_UNREACHABLE() __asm__ volatile("ud2":::"memory");

extern HIDDEN ELF_DYNAMIC_ITEM _DYNAMIC[];
extern HIDDEN void* _GLOBAL_OFFSET_TABLE_[];

CLANG_WORKAROUND
HIDDEN
uintptr_t RtlGetImageBase()
{
	// The GOT's first entry is the link-time address of _DYNAMIC.
	uintptr_t AddressLT = (uintptr_t) _GLOBAL_OFFSET_TABLE_[0];
	uintptr_t AddressRT = (uintptr_t) _DYNAMIC;
	return AddressRT - AddressLT;
}

extern NO_RETURN HIDDEN void DLLEntryPoint(PPEB Peb);

// This allows the shared library to relocate itself automatically.
//
// Note that the code is very fragile, and as such, you should probably
// not touch this unless you know what you are doing.  This code does not
// make references to any external functions.  As such it duplicates some
// functions that it would otherwise use.
//
// When mapping Libboron.so, **make every segment CoW!**  Libboron.so
// will then correct the permissions on itself.
//
// Thanks to https://github.com/managarm/mlibc for the inspiration.
//
// TODO: Consider using automated testing to check if this function
// contains relocations.
//
// This is the entry point of Libboron.so.
//
// Note: Entry points for libraries are NOT supposed to be called when
// loading them as shared libraries.  Instead, the loader should use
// .init_array.
NO_RETURN
void RelocateSelf(PPEB Peb)
{
	size_t RelOffset = 0, RelSize = 0, RelaOffset = 0, RelaSize = 0, RelrOffset = 0, RelrSize = 0;
	
	PELF_DYNAMIC_ITEM DynItem = _DYNAMIC;
	for (; DynItem->Tag != DYN_NULL; DynItem++)
	{
		switch (DynItem->Tag)
		{
			case DYN_REL:
				RelOffset = DynItem->Pointer;
				break;
			case DYN_RELSZ:
				RelSize = DynItem->Value;
				break;
			case DYN_RELA:
				RelaOffset = DynItem->Pointer;
				break;
			case DYN_RELASZ:
				RelaSize = DynItem->Value;
				break;
			case DYN_RELR:
				RelrOffset = DynItem->Pointer;
				break;
			case DYN_RELRSZ:
				RelrSize = DynItem->Value;
				break;
		}
	}
	
	uintptr_t ImageBase = RtlGetImageBase();
	
	for (size_t i = 0; i < RelaSize; i += sizeof(ELF_RELA))
	{
		PELF_RELA Rela = (PELF_RELA) (ImageBase + RelaOffset + i);
		
		uint32_t RelType = ELF_R_TYPE(Rela->Info);
		if (ELF_R_SYM(Rela->Info))
			BUG_UNREACHABLE();
		
	#if   defined TARGET_AMD64
		if (RelType != R_X86_64_RELATIVE)
	#elif defined TARGET_I386
		if (RelType != R_386_RELATIVE)
	#else
		#error TODO!
	#endif
			// Libboron.so, in addition to being restricted in so many
			// different ways already, also cannot import things from
			// other libraries.  As such, the only relocation type you
			// should see is R_*_RELATIVE.
			BUG_UNREACHABLE();
		
		uintptr_t* Reloc = (uintptr_t*) (ImageBase + Rela->Offset);
		*Reloc = ImageBase + Rela->Addend;
	}
	
	for (size_t i = 0; i < RelSize; i += sizeof(ELF_REL))
	{
		PELF_REL Rel = (PELF_REL) (ImageBase + RelOffset + i);
		
		uint32_t RelType = ELF_R_TYPE(Rel->Info);
		if (ELF_R_SYM(Rel->Info)) {
			// some symbol here has an info
			BUG_UNREACHABLE();
		}
		
	#if   defined TARGET_AMD64
		if (RelType != R_X86_64_RELATIVE)
	#elif defined TARGET_I386
		if (RelType != R_386_RELATIVE)
	#else
		#error TODO!
	#endif
			// Libboron.so, in addition to being restricted in so many
			// different ways already, also cannot import things from
			// other libraries.  As such, the only relocation type you
			// should see is R_*_RELATIVE.
			BUG_UNREACHABLE();
		
		uintptr_t* Reloc = (uintptr_t*) (ImageBase + Rel->Offset);
		*Reloc += ImageBase;
	}
	
	// Thanks to mlibc for this one.
	uintptr_t* CurrentBaseAddress = NULL;
	
	for (size_t i = 0; i < RelrSize; i += sizeof(uintptr_t))
	{
		uintptr_t Rel = *(uintptr_t*) (ImageBase + RelrOffset + i);
		
		if (Rel & 1)
		{
			// Skip the first bit because we have already relocated
			// the first entry.
			Rel >>= 1;
			
			for (; Rel; Rel >>= 1)
			{
				if (Rel & 1)
					*CurrentBaseAddress += ImageBase;
				
				CurrentBaseAddress++;
			}
		}
		else
		{
			// Even entry indicates the beginning address of the range
			// to relocate.
			CurrentBaseAddress = (uintptr_t*) (ImageBase + Rel);
			
			// Then relocate the first entry.
			*CurrentBaseAddress += ImageBase;
			CurrentBaseAddress++;
		}
	}
	
	// Finally, run the entry point.
	DLLEntryPoint(Peb);
}
