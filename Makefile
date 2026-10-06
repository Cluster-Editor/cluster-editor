BUILD_DIR ?= build
BUILD_TYPE ?= Debug

.PHONY: all configure build test run dev format clean

all: build


configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCOLLAB_EDITOR_BUILD_TESTS=ON

build: configure
	cmake --build $(BUILD_DIR) --config $(BUILD_TYPE)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure -C $(BUILD_TYPE)

run: build
	$(BUILD_DIR)/collab_server --port 9001

dev:
	docker compose up --build

format:
	@echo "Formatting will be enabled when the CRDT sources are added."

clean:
	cmake -E remove_directory $(BUILD_DIR)
