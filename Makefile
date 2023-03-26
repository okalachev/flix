SKETCH = flix
BOARD := esp32:esp32:d1_mini32
FILE = arduino.avr.nano
PORT := /dev/cu.usbserial-01E2D770

build:
# arduino-cli compile --fqbn $(BOARD) --build-path $(SKETCH)/build --build-cache-path $(SKETCH)/cache $(SKETCH)
	arduino-cli compile --fqbn $(BOARD) $(SKETCH)

upload: build
	arduino-cli upload --fqbn $(BOARD) -p $(PORT) $(SKETCH)

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=115200

dependencies:
	arduino-cli core update-index
	arduino-cli core install esp32:esp32
	arduino-cli lib install 'Bolder Flight Systems SBUS'@1.0.1
	arduino-cli lib install --git-url https://github.com/okalachev/MPU9250.git

gazebo/build cmake: gazebo/CMakeLists.txt
	mkdir -p gazebo/build
	cd gazebo/build && cmake ..

build_simulator: gazebo/build
	make -C gazebo/build

simulator: build_simulator
	GAZEBO_MODEL_PATH=$$GAZEBO_MODEL_PATH:${CURDIR}/gazebo/models \
	GAZEBO_PLUGIN_PATH=$$GAZEBO_PLUGIN_PATH:${CURDIR}/gazebo/build \
	gazebo --verbose ${CURDIR}/gazebo/flix.world

grab_log:
	tools/grab_log.py

clean:
	rm -rf gazebo/plugin/build $(SKETCH)/build $(SKETCH)/cache

.PHONY: build upload monitor upload_and_monitor dependencies cmake build_simulator simulator grab_log clean
