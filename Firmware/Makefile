# Flix 固件构建脚本（耀坤复刻版目录整理后）
#
# 【为什么本文件在 Firmware/ 下】
#   目录整理后 flix/（Arduino sketch）与 gazebo/（Gazebo 仿真器）作为"完整可构建工程"
#   一起放进 Firmware/，两者相对路径不变，因此下面所有 `flix` / `gazebo` 的相对引用
#   一条都不用改；而原来指向 `tools/` 的引用变成 `../Tools/`（工具脚本已移出固件目录）。
#
# 【怎么用】
#   在仓库根目录执行 `make ...`（根 Makefile 会自动转发到这里），
#   或直接 `cd Firmware && make ...`，效果完全相同。
BOARD = esp32:esp32:esp32
PORT = $(strip $(wildcard /dev/serial/by-id/usb-Silicon_Labs_CP21* /dev/serial/by-id/usb-1a86_USB_Single_Serial_* /dev/cu.usbserial-* /dev/cu.usbmodem*))

export ARDUINO_NETWORK_CONNECTION_TIMEOUT = 1h

build: .core .libs
	arduino-cli compile flix --fqbn $(BOARD) --build-property "build.core_debug_level=1" $(EXTRA)

upload: build
	arduino-cli upload flix --fqbn $(BOARD) -p "$(PORT)"

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
	arduino-cli lib install "MAVLink"@2.0.33
	touch .libs

upload_proxy: .core .libs
	arduino-cli compile ../Tools/espnow-proxy --fqbn $(BOARD)
	arduino-cli upload ../Tools/espnow-proxy --fqbn $(BOARD) -p "$(PORT)"

gazebo/build cmake: gazebo/CMakeLists.txt
	mkdir -p gazebo/build
	cd gazebo/build && cmake ..

build_simulator: .libs gazebo/build
	make -C gazebo/build

simulator: build_simulator
	GAZEBO_MODEL_PATH=$$GAZEBO_MODEL_PATH:${CURDIR}/gazebo/models \
	GAZEBO_PLUGIN_PATH=$$GAZEBO_PLUGIN_PATH:${CURDIR}/gazebo/build \
	gazebo --verbose ${CURDIR}/gazebo/flix.world

log:
	../Tools/log.py

plot:
	plotjuggler -d $(shell ls -t ../Tools/log/*.csv | head -n1)

clean:
	rm -rf gazebo/build flix/build flix/cache .core .libs

.PHONY: build upload monitor core libs cmake build_simulator simulator log clean
