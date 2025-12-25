
run: image
	@echo "Running..."
	@./tools/run-unix-armv6.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./tools/run-armv6.sh
