# Simple Makefile wrapper for CMake build

.PHONY: all build clean configure run stop test help

# PID file for background process
PID_FILE = .url_shortener.pid

# Default target
all: build

# Build the project
build:
	@cd build && cmake --build . -j

# Configure CMake (run this once or when CMakeLists.txt changes)
configure:
	@mkdir -p build
	@cd build && cmake ..

# Clean build artifacts
clean:
	@cd build && cmake --build . --target clean

# Full clean (remove build directory)
distclean:
	@rm -rf build

# Run the application in background
run: build stop
	@echo "Starting URL shortener in background..."
	@nohup ./url_shortener > server.log 2>&1 & echo $$! > $(PID_FILE)
	@sleep 1
	@if [ -f $(PID_FILE) ] && kill -0 `cat $(PID_FILE)` 2>/dev/null; then \
		echo "✅ Server started successfully (PID: `cat $(PID_FILE)`)"; \
		echo "📋 Logs: tail -f server.log"; \
	else \
		echo "❌ Failed to start server"; \
		rm -f $(PID_FILE); \
		exit 1; \
	fi

# Stop the application
stop:
	@if [ -f $(PID_FILE) ]; then \
		PID=`cat $(PID_FILE)`; \
		if kill -0 $$PID 2>/dev/null; then \
			echo "Stopping URL shortener (PID: $$PID)..."; \
			kill $$PID; \
			sleep 1; \
			if kill -0 $$PID 2>/dev/null; then \
				echo "Force killing..."; \
				kill -9 $$PID; \
			fi; \
			echo "✅ Server stopped"; \
		else \
			echo "Server not running"; \
		fi; \
		rm -f $(PID_FILE); \
	else \
		echo "Server not running (no PID file)"; \
	fi

# Check server status
status:
	@if [ -f $(PID_FILE) ]; then \
		PID=`cat $(PID_FILE)`; \
		if kill -0 $$PID 2>/dev/null; then \
			echo "✅ Server is running (PID: $$PID)"; \
		else \
			echo "❌ Server is not running (stale PID file)"; \
			rm -f $(PID_FILE); \
		fi; \
	else \
		echo "❌ Server is not running"; \
	fi

# Run API tests (starts server if needed)
test: 
	@if ! [ -f $(PID_FILE) ] || ! kill -0 `cat $(PID_FILE)` 2>/dev/null; then \
		echo "Starting server for testing..."; \
		$(MAKE) run; \
		sleep 2; \
	fi
	@./test_api.sh

# Setup: configure and build
setup: configure build

# Help
help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  build      - Build the project"
	@echo "  configure  - Configure CMake (run once)"
	@echo "  clean      - Clean build artifacts"
	@echo "  distclean  - Remove entire build directory"
	@echo "  run        - Build and start server in background"
	@echo "  stop       - Stop the background server"
	@echo "  status     - Check if server is running"
	@echo "  test       - Run API tests (starts server if needed)"
	@echo "  setup      - Configure and build (first time)"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Server management:"
	@echo "  make run    - Start server in background"
	@echo "  make stop   - Stop server"
	@echo "  make status - Check server status"
	@echo "  tail -f server.log - View server logs"