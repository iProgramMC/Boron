// Boron DLL
#include <boron.h>
#include <elf.h>

extern __attribute__((visibility("hidden"))) ELF_DYNAMIC_ITEM _DYNAMIC[];
extern __attribute__((visibility("hidden"))) void* _GLOBAL_OFFSET_TABLE_[];

static uintptr_t RtlpGetImageBase()
{
	// The GOT's first entry is the link-time address of _DYNAMIC.
	uintptr_t AddressLT = (uintptr_t) _GLOBAL_OFFSET_TABLE_[0];
	uintptr_t AddressRT = (uintptr_t) _DYNAMIC;
	return AddressRT - AddressLT;
}

extern HIDDEN void DLLEntryPoint();

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
// needs to be relocated.
//
// This is the entry point of Libboron.so.
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
	
	uintptr_t ImageBase = RtlpGetImageBase();
	
	for (size_t i = 0; i < RelaSize; i += sizeof(ELF_RELA))
	{
		PELF_RELA Rela = (PELF_RELA) (ImageBase + RelaOffset + i);
		
	#ifndef TARGET_AMD64
	#error TODO!
	#else
		uint32_t RelType = (uint32_t) Rela->Info;
		
		if (RelType != R_X86_64_RELATIVE)
			// Libboron.so, in addition to being restricted in so many
			// different ways already, also cannot import things from
			// other libraries.  As such, the only relocation type you
			// should see is R_X86_64_RELATIVE.
			__builtin_trap();
		
		uintptr_t* Reloc = (uintptr_t*) (ImageBase + Rela->Offset);
		*Reloc = ImageBase + Rela->Addend;
	#endif
	}
	
	for (size_t i = 0; i < RelSize; i += sizeof(ELF_REL))
	{
		PELF_REL Rel = (PELF_REL) (ImageBase + RelOffset + i);
		
	#ifndef TARGET_AMD64
	#error TODO!
	#else
		uint32_t RelType = (uint32_t) Rel->Info;
		
		if (RelType != R_X86_64_RELATIVE)
			// Libboron.so, in addition to being restricted in so many
			// different ways already, also cannot import things from
			// other libraries.  As such, the only relocation type you
			// should see is R_X86_64_RELATIVE.
			__builtin_trap();
		
		uintptr_t* Reloc = (uintptr_t*) (ImageBase + Rel->Offset);
		*Reloc += ImageBase;
	#endif
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
