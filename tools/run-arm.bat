@rem Run script 

@echo off

set backupPath=%path%
set NSPath=%CD%
cd /d c:\Program Files\qemu
set path=%path%;%NSPath%

@rem NOTE: This will likely only run on iProgramInCpp's machine for now.
@rem Just use the i386 or amd64 builds...

set ipodDataPath=W:\iphone-qemu\ipod-data
set ipodQemuPath=W:\iphone-qemu\qemu-ios\build

set qemu=%ipodQemuPath%\qemu-system-arm.exe
set ibootPath=%ipodDataPath%\ipt_1g_openiboot.dec
set nandPath=%ipodDataPath%\nand.img
set oibElfPath=%NSPath%\build\image.arm.tar
set bootromPath=%ipodDataPath%\bootrom_s5l8900
set norPath=%ipodDataPath%\nor_n45ap.bin

set machineType=iPod-Touch,bootrom=%bootromPath%,iboot=%ibootPath%,nand=%nandPath%,oib-elf=%oibElfPath%

%qemu% ^
	-M %machineType% ^
	-serial stdio ^
	-cpu max ^
	-m 1G ^
	-d unimp ^
	-pflash %norPath% ^
	-display sdl ^
	-monitor telnet:127.0.0.1:56789,server,nowait ^
	-no-reboot ^
	-no-shutdown ^
	-s

rem go back
cd /d %NSPath%

set path=%backupPath%
