
run: image
	@echo "Running..."
	@./tools/run-unix-arm.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./tools/run-arm.sh
