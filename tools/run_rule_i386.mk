
run: image
	@echo "Running..."
	@./tools/run-unix-i386.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./tools/run-i386.sh
