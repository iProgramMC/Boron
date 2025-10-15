qemu-system-i386 \
    -no-reboot \
    -no-shutdown \
    -M q35 \
    -m 256M \
    -smp 4 \
    -boot d \
    -cdrom build/image.i386.iso \
    -debugcon stdio
