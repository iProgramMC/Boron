
run: image
	@echo "Running..."
	@./tools/run-unix-amd64.sh

runw: image
	@echo "Invoking WSL to run the OS..."
	@./tools/run-amd64.sh
