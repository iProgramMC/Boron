@echo off
:a
"C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" debugvm "64-bit Misc" getregisters --cpu=2 rip
goto a
