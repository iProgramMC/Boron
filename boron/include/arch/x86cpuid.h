#pragma once

typedef struct
{
	uint32_t Eax;
	uint32_t Ebx;
	struct {
		unsigned Sse3 : 1;       // 0
		unsigned Pclmulqdq : 1;  // 1
		unsigned Dtes64 : 1;     // 2
		unsigned Monitor : 1;    // 3
		unsigned DsCpl : 1;      // 4
		unsigned Vmx : 1;        // 5
		unsigned Smx : 1;        // 6
		unsigned Eist : 1;       // 7
		unsigned Tm2 : 1;        // 8
		unsigned Ssse3 : 1;      // 9
		unsigned CnxtId : 1;     // 10
		unsigned Sdbg : 1;       // 11
		unsigned Fma : 1;        // 12
		unsigned Cmpxchg16b : 1; // 13
		unsigned XtprUC : 1;     // 14
		unsigned Pdcm : 1;       // 15
		unsigned Reserved : 1;   // 16
		unsigned Pcid : 1;       // 17
		unsigned Dca : 1;        // 18
		unsigned Sse41 : 1;      // 19
		unsigned Sse42 : 1;      // 20
		unsigned X2apic : 1;     // 21
		unsigned Movbe : 1;      // 22
		unsigned Popcnt : 1;     // 23
		unsigned TscDline : 1;   // 24
		unsigned Aesni : 1;      // 25
		unsigned Xsave : 1;      // 26
		unsigned Osxsave : 1;    // 27
		unsigned Avx : 1;        // 28
		unsigned F16c : 1;       // 29
		unsigned Rdrand : 1;     // 30
		unsigned Unused : 1;     // 31
	}
	PACKED
	Ecx;
	struct {
		unsigned Fpu : 1;        // 0
		unsigned Vme : 1;        // 1
		unsigned De : 1;         // 2
		unsigned Pse : 1;        // 3
		unsigned Tsc : 1;        // 4
		unsigned Msr : 1;        // 5
		unsigned Pae : 1;        // 6
		unsigned Mce : 1;        // 7
		unsigned Cx8 : 1;        // 8
		unsigned Apic : 1;       // 9
		unsigned Reserved : 1;   // 10
		unsigned Sep : 1;        // 11
		unsigned Mtrr : 1;       // 12
		unsigned Pge : 1;        // 13
		unsigned Mca : 1;        // 14
		unsigned Cmov : 1;       // 15
		unsigned Pat : 1;        // 16
		unsigned Pse36 : 1;      // 17
		unsigned Psn : 1;        // 18
		unsigned Clfsh : 1;      // 19
		unsigned Reserved2 : 1;  // 20
		unsigned Ds : 1;         // 21
		unsigned Acpi : 1;       // 22
		unsigned Mmx : 1;        // 23
		unsigned Fxsr : 1;       // 24
		unsigned Sse : 1;        // 25
		unsigned Sse2 : 1;       // 26
		unsigned Ss : 1;         // 27
		unsigned Htt : 1;        // 28
		unsigned Tm : 1;         // 29
		unsigned Reserved3 : 1;  // 30
		unsigned Pbe : 1;        // 31
	}
	PACKED
	Edx;
}
PACKED
CPUID_EAX01H_OUTPUT;

typedef union
{
	struct
	{
		uint32_t Eax;
		uint32_t Ebx;
		uint32_t Ecx;
		uint32_t Edx;
	}
	Bare;
	
	CPUID_EAX01H_OUTPUT Eax01H;
}
CPUID_OUTPUT, *PCPUID_OUTPUT;

static_assert(sizeof(CPUID_OUTPUT) == 16);
static_assert(sizeof(CPUID_EAX01H_OUTPUT) == 16);
