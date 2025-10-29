
$(IMAGE_TARGET): kernel drivers apps limine_config
	@echo "[MK]\tBuilding iso..."
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@cp $(KERNEL_ELF) $(ISO_DIR)/$(KERNEL_NAME)
	@cp -r $(BUILD_DIR)/*.exe $(BUILD_DIR)/*.sys $(BUILD_DIR)/*.so limine/limine-bios.sys limine/limine-bios-cd.bin $(ISO_DIR)
	@cp limine.$(TARGETL).conf $(ISO_DIR)/limine.conf
	@xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --protective-msdos-label $(ISO_DIR) -o $@ 2>/dev/null
	@limine/limine-deploy $@ 2>/dev/null
	@rm -rf $(ISO_DIR)

limine_config:  limine.$(TARGETL).conf
	@echo "[MK]\tlimine.$(TARGETL).conf was updated"
