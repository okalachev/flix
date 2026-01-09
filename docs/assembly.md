# Brief assembly guide

Soldered components ([schematics variant](https://miro.com/app/board/uXjVN-dTjoo=/?moveToWidget=3458764612338222067&cot=14)):

<img src="img/assembly/1.jpg" width=600>

<br>Use double-sided tape to attach ESP32 to the top frame part (ESP32 holder):

<img src="img/assembly/2.jpg" width=600>

<br>Use two washers to screw the IMU board to the frame:

<img src="img/assembly/3.jpg" width=600>

<br>Screw the IMU with M3x5 screws as shown:

<img src="img/assembly/4.jpg" width=600>

<br>Install the motors, attach MOSFETs to the frame using tape:

<img src="img/assembly/5.jpg" width=600>

<br>Screw the ESP32 holder with M1.4x5 screws to the frame:

<img src="img/assembly/6.jpg" width=600>

<br>Assembled drone:

<img src="img/assembly/7.jpg" width=600>

## Motor directions

> [!WARNING]
> The drone above is an early build, and it has **inversed** motor directions scheme. The photos only illustrate the assembly process in general.

Use standard motor directions scheme:

<img src="img/motors.svg" width=200>

Motors connection table:

|Motor|Position|Direction|Prop type|Motor wires|GPIO|
|-|-|-|-|-|-|
|Motor 0|Rear left|Counter-clockwise|B|Black & White|GPIO12 (*TDI*)|
|Motor 1|Rear right|Clockwise|A|Blue & Red|GPIO13 (*TCK*)|
|Motor 2|Front right|Counter-clockwise|B|Black & White|GPIO14 (*TMS*)|
|Motor 3|Front left|Clockwise|A|Blue & Red|GPIO15 (*TD0*)|

## Motors tightening

Motors should be installed very tightly — any vibration may lead to bad attitude estimation and unstable flight. If motors are loose, use tiny tape pieces to fix them tightly as shown below:

<img src="img/motor-tape.jpg" width=600>
