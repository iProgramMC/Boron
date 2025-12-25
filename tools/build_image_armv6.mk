# TEMPORARY

$(IMAGE_TARGET): kernel drivers apps initrd
	@echo "[MK]\tBuilding iso..."
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_ELF) $(ISO_DIR)/$(KERNEL_NAME)
	@cp -r $(BUILD_DIR)/*.sys $(BUILD_DIR)/*.tar $(ISO_DIR)
	@xorriso -as mkisofs -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label $(ISO_DIR) -o $@ 2>/dev/null
	@rm -rf $(ISO_DIR)
