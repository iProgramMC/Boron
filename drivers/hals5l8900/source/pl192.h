#pragma once

#define PL192_MAX_INTERRUPTS_TOTAL 64

// N.B.  Each PL192 has support for 32 interrupts, so 64 total
#define MAX_INTERRUPTS PL192_MAX_INTERRUPTS_TOTAL

#define VIC0_BASE_PHYS (0x38E00000)
#define VIC1_BASE_PHYS (0x38E01000)

#define VICREG(n, rgof) (* (volatile uint32_t*) ((uintptr_t)HalVic ## n ## Base + rgof))

#define VICIRQSTATUS(n)      VICREG(n, 0x000)
#define VICFIQSTATUS(n)      VICREG(n, 0x004)
#define VICRAWINTR(n)        VICREG(n, 0x008)
#define VICINTSELECT(n)      VICREG(n, 0x00C)
#define VICINTENABLE(n)      VICREG(n, 0x010)
#define VICINTENCLEAR(n)     VICREG(n, 0x014)
#define VICSOFTINT(n)        VICREG(n, 0x018)
#define VICSOFTINTCLEAR(n)   VICREG(n, 0x01C)
#define VICPROTECTION(n)     VICREG(n, 0x020)
#define VICSWPRIORITYMASK(n) VICREG(n, 0x024)
#define VICPRIORITYDAISY(n)  VICREG(n, 0x028)
#define VICVECTADDR(n)     (&VICREG(n, 0x100)) // index as VICVECTADDR(VIC#)[INT#].  INT# in [0,31]
#define VICVECTPRIORITY(n) (&VICREG(n, 0x200)) // index as VICVECTPRIORITY(VIC#)[INT#].  INT# in [0,31]
#define VICADDRESS(n)        VICREG(n, 0xF00)  // weird place but OK

void HalInitPL192();

int HalGetMaximumInterruptCount();

void HalVicRegisterInterrupt(int InterruptNumber, KIPL Ipl);

void HalVicDeregisterInterrupt(int InterruptNumber, UNUSED KIPL Ipl);

void HalOnUpdateIpl(KIPL NewIpl, UNUSED KIPL OldIpl);

void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);

void HalEndOfInterrupt(int InterruptNumber);

KIPL HalEnterHardwareInterrupt(int InterruptNumber);

void HalExitHardwareInterrupt(int InterruptNumber, KIPL OldIpl);

PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers);

PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers);
