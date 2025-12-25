@rem Run script 

@echo off

set backupPath=%path%
set NSPath=%CD%
cd /d c:\Program Files\qemu
set path=%path%;%NSPath%

@rem NOTE: Placeholder !!
qemu-system-arm.exe -no-reboot -no-shutdown -d int ^
-M virt ^
-cpu arm1176 ^
-m 256M ^
-kernel %nspath%\build\armv6\kernel.elf

rem go back
cd /d %NSPath%

set path=%backupPath%
