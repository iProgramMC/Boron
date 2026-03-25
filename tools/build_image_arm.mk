# TEMPORARY

$(IMAGE_TARGET): kernel drivers apps initrd
	@echo "[MK]\tBuilding tar file"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_ELF) $(ISO_DIR)/$(KERNEL_NAME)
	@cp -r $(BUILD_DIR)/*.sys $(BUILD_DIR)/*.tar $(ISO_DIR)
	@tar -cvf $(IMAGE_TARGET) -C $(ISO_DIR) . > /dev/null
	@rm -rf $(ISO_DIR)
