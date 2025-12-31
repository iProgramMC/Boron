# TEMPORARY

$(IMAGE_TARGET): kernel drivers apps initrd
	@echo "[MK]\tBuilding iso..."
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)
	cp $(KERNEL_ELF) $(ISO_DIR)/$(KERNEL_NAME)
	cp -r $(BUILD_DIR)/*.sys $(BUILD_DIR)/*.tar $(ISO_DIR)	
	dd if=/dev/zero of=$(IMAGE_TARGET) bs=1M count=64
	mkfs.vfat $(IMAGE_TARGET)
	mcopy -i $(IMAGE_TARGET) -s $(ISO_DIR)/* ::

