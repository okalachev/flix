# Adaptación para ESP32-C3 Super Mini

Registro de cambios, problemas detectados y trabajo pendiente para portar Flix al ESP32-C3 Super Mini.

---

## Diferencias clave vs ESP32 original (d1_mini32)

| Aspecto | ESP32 (original) | ESP32-C3 Super Mini |
|---|---|---|
| Arquitectura | Xtensa LX6 (dual core) | RISC-V (single core) |
| GPIOs disponibles | 0–39 (varios expuestos) | 0–10, 20, 21 |
| UARTs | UART0, UART1, UART2 | UART0, UART1 (no UART2) |
| SPI por defecto | SCK=18, MISO=19, MOSI=23, SS=5 | SCK=4, MISO=5, MOSI=6, SS=7 |
| LED integrado | GPIO 2 | GPIO 8 |
| Botón boot | GPIO 0 | GPIO 9 |
| USB-Serial | Chip externo CP2102 | USB-CDC integrado (nativo) |

Fuentes: [FlixPeriph README](https://github.com/okalachev/flixperiph),
[arduino-esp32 variants/esp32c3/pins_arduino.h](https://github.com/espressif/arduino-esp32/blob/master/variants/esp32c3/pins_arduino.h)

---

## Pinout del ESP32-C3 Super Mini

```
         USB
    ┌────┤├────┐
    │ 5V    G  │
    │ G     5V │
    │3V3   3V3 │
    │ G     G  │
    │ 1    10  │  → motores M0 y M3
    │ 0(⚠)  9(⚠)│  ⚠ strapping / boot pin
    │ 2     8  │  → motor M1 / LED integrado
    │ 3     7  │  → motor M2 / SPI SS (IMU)
    │ 4     6  │  SPI SCK / SPI MOSI (IMU)
    │ 5    20  │  SPI MISO / UART TX
    │ G    21  │  UART RX
    └──────────┘
```

---

## Cambios realizados

### 1. Pines de motores (`flix/motors.ino`)

Los GPIOs 12–15 no existen en el Super Mini. Se reasignan a los únicos 4 pines
libres que no colisionan con SPI, LED, boot ni UART:

| Motor | Pin original | Pin ESP32-C3 | Notas |
|---|---|---|---|
| Rear-left (M0) | GPIO 12 | GPIO 1 | Libre |
| Rear-right (M1) | GPIO 13 | GPIO 2 | Libre |
| Front-right (M2) | GPIO 14 | GPIO 3 | Libre |
| Front-left (M3) | GPIO 15 | GPIO 10 | Libre |

Pines descartados y razón:
- GPIO 0: pin strapping — pull-down del MOSFET lo forzaría a LOW en boot (modo descarga)
- GPIO 4/5/6/7: bus SPI del IMU (SCK, MISO, MOSI, SS)
- GPIO 8: LED integrado (usado por `led.ino`)
- GPIO 9: botón boot con pull-up interno
- GPIO 20/21: UART0 (Serial/USB-CDC)

### 2. Puerto serie para RC (`flix/rc.ino`)

El ESP32-C3 no tiene UART2. Cambio incluido en `origin/esp32-c3`:

```cpp
#ifdef ESP32C3
SBUS rc(Serial1);   // UART1
#else
SBUS rc(Serial2);   // UART2
#endif
```

### 3. PlatformIO (`platformio.ini`)

- Board: `esp32-c3-devkitm-1`
- Flag `-D ESP32C3` activa los `#ifdef` del firmware
- `-D ARDUINO_USB_CDC_ON_BOOT=1` necesario para que `Serial.print` salga por USB nativo

---

## Trabajo pendiente

### Alta prioridad

- [ ] **Configurar pin RX de Serial1 para RC (SBUS)**

  En ESP32-C3, Serial1 no tiene pines asignados por defecto — hay que asignarlos
  explícitamente. Añadir en `rc.ino` o `flix.ino`:

  ```cpp
  #ifdef ESP32C3
  #define PIN_RC_RX 9  // candidato: botón boot, confirmar con hardware
  Serial1.begin(100000, SERIAL_8E2, PIN_RC_RX, -1, true);
  #endif
  ```

  El único pin disponible para RC es GPIO 9 (botón boot). Tiene pull-up interno
  lo que puede interferir con SBUS. Si no se usa RC físico no es bloqueante.

- [ ] **Verificar SPI del IMU en ESP32-C3**

  `imu.ino` llama `MPU9250 imu(SPI)` sin especificar pines. La librería delega
  en el objeto `SPI` del Arduino core, que en ESP32-C3 usa SCK=4, MISO=5, MOSI=6, SS=7.
  Estos pines no colisionan con nada asignado actualmente.
  **Acción:** compilar y verificar que el IMU responde correctamente en esos pines.

### Media prioridad

- [ ] **Validar `disableBrownOut()`** (`flix.ino`)

  Usa la API ESP-IDF interna. Verificar que compila en ESP32-C3 o añadir `#ifdef`.

- [ ] **GPIO 9 como RC vs botón boot**

  GPIO 9 es el único pin libre restante pero tiene pull-up a 3V3.
  Evaluar si el nivel en reposo es compatible con el protocolo SBUS (inverted UART).

### Baja prioridad

- [ ] **CI/CD** (`.github/workflows/build.yml`)

  El repo original ya compila para ESP32-C3 con Arduino CLI.
  Evaluar si añadir un job con PlatformIO o mantener los dos sistemas en paralelo.

---

## Mapa de pines final (validado)

| Función | GPIO | Estado |
|---|---|---|
| Motor rear-left (M0) | 1 | Asignado |
| Motor rear-right (M1) | 2 | Asignado |
| Motor front-right (M2) | 3 | Asignado |
| Motor front-left (M3) | 10 | Asignado |
| SPI SCK (IMU) | 4 | Default arduino-esp32 |
| SPI MISO (IMU) | 5 | Default arduino-esp32 |
| SPI MOSI (IMU) | 6 | Default arduino-esp32 |
| SPI SS/CS (IMU) | 7 | Default arduino-esp32 |
| LED integrado | 8 | Default LED_BUILTIN |
| RC SBUS RX (Serial1) | 9 | Pendiente — boot pin, evaluar |
| UART TX (Serial/debug) | 21 | Default UART0 |
| UART RX (Serial/debug) | 20 | Default UART0 |
