# Top-level Makefile for Google ML Kit Barcode Linux & Jetson Port

.PHONY: all build build-shim build-cpp test clean help

all: build

help:
	@echo "Google ML Kit Linux Port Build System"
	@echo "Targets:"
	@echo "  make build       - Build Bionic shim and C++ SDK demo"
	@echo "  make build-shim  - Build libandroid_shim.so"
	@echo "  make build-cpp   - Build native C++ SDK library and demo binary"
	@echo "  make test        - Run Python and C++ test suite on sample barcodes"
	@echo "  make clean       - Clean all compiled binaries and CMake caches"

build: build-shim build-cpp

build-shim:
	@echo "[+] Building Bionic translation shim..."
	@bash core/build.sh

build-cpp:
	@echo "[+] Building C++ SDK and demo..."
	@mkdir -p sdk/cpp/build
	@cd sdk/cpp/build && cmake .. && make -j$$(nproc)

test: build
	@echo "[+] Running Python ML Kit scanner..."
	@python3 google_mlkit_scanner.py
	@echo "[+] Running C++ ML Kit demo..."
	@./sdk/cpp/build/mlkit_demo samples/sample_qr.bmp

clean:
	@echo "[+] Cleaning build artifacts..."
	@rm -rf sdk/cpp/build
	@rm -f core/lib/x86_64/libandroid_shim.so
	@rm -f core/lib/x86_64/liblog.so core/lib/x86_64/libjnigraphics.so
	@find . -name "*.pyc" -delete
	@find . -name "__pycache__" -delete
	@echo "[+] Clean complete."
