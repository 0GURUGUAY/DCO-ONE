.PHONY: help build build-daisy build-esp32 clean check-hardware upload-daisy upload-esp32 all

help:
	@echo "=== DCO-ONE Phase 1 Build System ==="
	@echo ""
	@echo "Available targets:"
	@echo "  make check-hardware  - Verify hardware connections"
	@echo "  make build-all       - Build both Daisy and ESP32-S3"
	@echo "  make build-daisy     - Build Daisy firmware only"
	@echo "  make build-esp32     - Build ESP32-S3 firmware only"
	@echo "  make upload-daisy    - Upload Daisy firmware (requires DFU mode)"
	@echo "  make upload-esp32    - Upload ESP32-S3 firmware"
	@echo "  make monitor-daisy   - Monitor Daisy serial output"
	@echo "  make monitor-esp32   - Monitor ESP32-S3 serial output"
	@echo "  make clean           - Clean all build artifacts"
	@echo ""

check-hardware:
	@echo "Running hardware diagnostic..."
	@bash check_hardware.sh

build-daisy:
	@echo "Building Daisy firmware..."
	@cd daisy && \
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make
	@echo "✓ Daisy build complete"

build-esp32:
	@echo "Building ESP32-S3 firmware..."
	@cd esp32 && \
	platformio run
	@echo "✓ ESP32-S3 build complete"

build-all: build-daisy build-esp32
	@echo "✓ All builds complete"

upload-daisy: build-daisy
	@echo "Uploading Daisy firmware (put device in DFU mode)"
	@echo "Uploading via DFU..."
	@arm-none-eabi-objcopy -O binary daisy/build/dco_one_phase1 /tmp/dco_one_phase1.bin
	@dfu-util -a 0 -D /tmp/dco_one_phase1.bin -s 0x08000000
	@echo "✓ Daisy upload complete"
	@echo "Device will reset automatically..."
	@sleep 2

upload-esp32:
	@echo "Uploading ESP32-S3 firmware..."
	@cd esp32 && \
	platformio run --target upload
	@echo "✓ ESP32-S3 upload complete"

monitor-daisy:
	@echo "Monitoring Daisy serial output..."
	@screen /dev/cu.usbserial-* 115200

monitor-esp32:
	@echo "Monitoring ESP32-S3 serial output..."
	@cd esp32 && \
	platformio device monitor

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf daisy/build
	@cd esp32 && platformio run --target clean
	@echo "✓ Clean complete"

.DEFAULT_GOAL := help
