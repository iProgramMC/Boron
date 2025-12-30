@rem Run script 

@echo off

set backupPath=%path%
set NSPath=%CD%
cd /d c:\Program Files\qemu
set path=%path%;%NSPath%

@rem NOTE: Placeholder !!
qemu-system-arm.exe ^
	-d int ^
	-M versatilepb ^
	-cpu arm1176 ^
	-m 128M ^
	-kernel %nspath%\build\armv6\kernel.elf ^
	-monitor telnet:127.0.0.1:56789,server,nowait ^
	-serial stdio ^
	-display sdl ^
	-no-reboot ^
	-no-shutdown ^
	-s

rem go back
cd /d %NSPath%

set path=%backupPath%
