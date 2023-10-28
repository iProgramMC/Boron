#ifndef BORON_ARCH_AMD64_ARCHI_H
#define BORON_ARCH_AMD64_ARCHI_H

extern
int KiVectorCrash,
    KiVectorTlbShootdown,
    KiVectorDpcIpi;

PKREGISTERS KiHandleDpcIpi(PKREGISTERS Regs);
PKREGISTERS KiHandleCrashIpi(PKREGISTERS Regs);
PKREGISTERS KiHandleTlbShootdownIpi(PKREGISTERS Regs);

#endif//BORON_ARCH_AMD64_ARCHI_H