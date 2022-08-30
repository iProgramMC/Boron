qemu-system-x86_64 \
    -no-reboot \
    -no-shutdown \
    -M q35 \
    -m 256M \
    -smp 4 \
    -boot d \
    -cdrom build/image.iso \
    -debugcon stdio
