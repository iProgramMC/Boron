#ifndef NS64_HAL_APIC_H
#define NS64_HAL_APIC_H

void HalSendIpi(uint32_t Processor, int Vector);
void HalBroadcastIpi(int Vector, bool IncludeSelf);
void HalApicEoi();
void HalEnableApic();

#endif//NS64_HAL_APIC_H
