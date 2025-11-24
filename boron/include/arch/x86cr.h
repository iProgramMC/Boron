#pragma once

#define CR0_PE (1 << 0)
#define CR0_MP (1 << 1)
#define CR0_EM (1 << 2)
#define CR0_TS (1 << 3)
#define CR0_ET (1 << 4)
#define CR0_NE (1 << 5)
#define CR0_WP (1 << 16)
#define CR0_AM (1 << 18)
#define CR0_NW (1 << 29)
#define CR0_CD (1 << 30)
#define CR0_PG (1 << 31)

#define CR4_VME        (1 << 0)
#define CR4_PVI        (1 << 1)
#define CR4_TSD        (1 << 2)
#define CR4_DE         (1 << 3)
#define CR4_PSE        (1 << 4)
#define CR4_PAE        (1 << 5)
#define CR4_MCE        (1 << 6)
#define CR4_PGE        (1 << 7)
#define CR4_PCE        (1 << 8)
#define CR4_OSFXSR     (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)
#define CR4_UMIP       (1 << 11)
#define CR4_LA57       (1 << 12)
#define CR4_VMXE       (1 << 13)
#define CR4_SMXE       (1 << 14)
#define CR4_FSGSBASE   (1 << 16)
#define CR4_PCIDE      (1 << 17)
#define CR4_OSXSAVE    (1 << 18)
#define CR4_KL         (1 << 19)
#define CR4_SMEP       (1 << 20)
#define CR4_SMAP       (1 << 21)
#define CR4_PKE        (1 << 22)
#define CR4_CET        (1 << 23)
#define CR4_PKS        (1 << 24)
#define CR4_UINTR      (1 << 25)

static ALWAYS_INLINE inline
uintptr_t KiReadCR0()
{
	uintptr_t Val;
	ASM("mov %%cr0, %0" : "=r"(Val) : : "memory");
	return Val;
}

static ALWAYS_INLINE inline
uintptr_t KiReadCR4()
{
	uintptr_t Val;
	ASM("mov %%cr4, %0" : "=r"(Val) : : "memory");
	return Val;
}

static ALWAYS_INLINE inline
void KiWriteCR0(uintptr_t Cr0)
{
	ASM("mov %0, %%cr0" : : "r"(Cr0) : "memory");
}

static ALWAYS_INLINE inline
void KiWriteCR4(uintptr_t Cr4)
{
	ASM("mov %0, %%cr4" : : "r"(Cr4) : "memory");
}
