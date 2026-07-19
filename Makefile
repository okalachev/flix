BOARD = esp32:esp32:d1_mini32:DebugLevel=error
PORT := $(strip $(wildcard /dev/serial/by-id/usb-Silicon_Labs_CP21* /dev/serial/by-id/usb-1a86_USB_Single_Serial_* /dev/cu.usbserial-* /dev/cu.usbmodem*))

export ARDUINO_NETWORK_CONNECTION_TIMEOUT := 1h

build: .core .libs
	arduino-cli compile --fqbn $(BOARD) flix

upload: build
	arduino-cli upload --fqbn $(BOARD) -p "$(PORT)" flix

erase:
	arduino-cli burn-bootloader --fqbn $(BOARD) -p "$(PORT)" -P esptool

monitor:
	arduino-cli monitor -p "$(PORT)" -c baudrate=115200

core .core:
	arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
	arduino-cli core install esp32:esp32@3.3.10 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
	touch .core

libs .libs:
	arduino-cli lib update-index
	arduino-cli lib install "FlixPeriph"
	arduino-cli lib install "MAVLink"@2.0.25
	touch .libs

upload_proxy: .core .libs
	arduino-cli compile --fqbn $(BOARD) tools/espnow-proxy
	arduino-cli upload --fqbn $(BOARD) -p "$(PORT)" tools/espnow-proxy

gazebo/build cmake: gazebo/CMakeLists.txt
	mkdir -p gazebo/build
	cd gazebo/build && cmake ..

build_simulator: .libs gazebo/build
	make -C gazebo/build

simulator: build_simulator
	GAZEBO_MODEL_PATH=$$GAZEBO_MODEL_PATH:${CURDIR}/gazebo/models \
	GAZEBO_PLUGIN_PATH=$$GAZEBO_PLUGIN_PATH:${CURDIR}/gazebo/build \
	gazebo --verbose ${CURDIR}/gazebo/flix.world

docker:
	xhost +local:docker
	GPU_DOCKER_ARG="--gpus all"; if ! command -v nvidia-smi > /dev/null 2>&1 || ! nvidia-smi > /dev/null 2>&1; then GPU_DOCKER_ARG=""; fi; \
	docker run -it --rm \
		--device=/dev/dri \
		-e DISPLAY=$$DISPLAY \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-v arduino_cache:/home/ubuntu/.config/arduino \
		-v arduino_data:/home/ubuntu/.arduino15 \
		-v arduino_home:/home/ubuntu/Arduino \
		-v ${CURDIR}:/flix \
		-w /flix \
		--network host \
		$$GPU_DOCKER_ARG \
		goldarte/gazebo-with-arduino \
		bash

log:
	tools/log.py

plot:
	plotjuggler -d $(shell ls -t tools/log/*.csv | head -n1)

clean:
	rm -rf gazebo/build flix/build flix/cache .core .libs

.PHONY: build upload monitor core libs cmake build_simulator simulator docker log clean
