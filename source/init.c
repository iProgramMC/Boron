#include <_limine.h>
#include <main.h>
#include <hal.h>
#include <mm.h>
#include <ke.h>

// bootloader requests
volatile struct limine_hhdm_request g_HHDMRequest =
{
	.id = LIMINE_HHDM_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_framebuffer_request g_FramebufferRequest =
{
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0,
	.response = NULL,
};
volatile struct limine_memmap_request g_MemMapRequest =
{
	.id       = LIMINE_MEMMAP_REQUEST,
	.revision = 0,
	.response = NULL,
};

ATOMIC bool fuck;

// The entry point to our kernel.
void KiSystemStartup(void)
{
	HalDebugTerminalInit();
	SLogMsg("Boron is starting up");
	
	HalTerminalInit();
	LogMsg("Boron (TM), August 2023 - V0.001");
	
	// initialize the physical memory manager
	MiInitPMM();
	
	int page1=MmAllocatePhysicalPage();
	int page2=MmAllocatePhysicalPage();
	int page3=MmAllocatePhysicalPage();
	
	MmFreePhysicalPage(page1);
	MmFreePhysicalPage(page3);
	MmFreePhysicalPage(page2);
	
	int page4=MmAllocatePhysicalPage();
	int page5=MmAllocatePhysicalPage();
	LogMsg("Allocate physical pages: %d %d",page4,page5);
	
	
	MmFreePhysicalPage(page5);
	MmFreePhysicalPage(page4);
	
	LogMsg("Stopping because we have nothing to do");
	KeStopCurrentCPU();
}

