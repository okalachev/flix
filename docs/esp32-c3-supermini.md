# Adaptación para ESP32-C3 Super Mini

Registro de cambios, problemas detectados y trabajo pendiente para portar Flix al ESP32-C3 Super Mini.

---

## Diferencias clave vs ESP32 original (d1_mini32)

| Aspecto | ESP32 (original) | ESP32-C3 Super Mini |
|---|---|---|
| Arquitectura | Xtensa LX6 (dual core) | RISC-V (single core) |
| GPIOs disponibles | 0–39 (varios expuestos) | 0–10, 20, 21 |
| UARTs | UART0, UART1, UART2 | UART0, UART1 (no UART2) |
| SPI por defecto | SCK=18, MISO=19, MOSI=23 | SCK=6, MISO=2, MOSI=7 |
| LED integrado | GPIO 2 | GPIO 8 (LED azul) |
| Botón boot | GPIO 0 | GPIO 9 |
| USB-Serial | Chip externo CP2102 | USB-CDC integrado (nativo) |

---

## Pinout del ESP32-C3 Super Mini

```
        USB
   ┌────┤├────┐
   │ 5V    G  │
   │ G     5V │
   │3V3   3V3 │
   │ G     G  │
   │ 1    10  │
   │0(⚠) 9(⚠)│  ⚠ = strapping/boot pin
   │ 2     8  │  8 = LED integrado
   │ 3     7  │  7 = SPI MOSI por defecto
   │ 4     6  │  6 = SPI SCK por defecto
   │ 5    20  │  5 = SPI MISO por defecto
   │ G    21  │  20,21 = UART0 (Serial)
   └──────────┘
```

---

## Cambios realizados

### 1. Pines de motores (`flix/motors.ino`)
Los GPIOs 12–15 del diseño original no existen en el Super Mini.

| Motor | Pin original | Pin ESP32-C3 |
|---|---|---|
| Rear-left (M0) | GPIO 12 | GPIO 4 |
| Rear-right (M1) | GPIO 13 | GPIO 5 |
| Front-right (M2) | GPIO 14 | GPIO 6 |
| Front-left (M3) | GPIO 15 | GPIO 7 |

> **⚠ Conflicto pendiente:** GPIO 5, 6, 7 son los pines SPI por defecto del ESP32-C3
> (MISO, SCK, MOSI). Si FlixPeriph inicializa SPI en esos pines, habrá conflicto con los motores.
> Ver sección "Trabajo pendiente".

### 2. Puerto serie para RC (`flix/rc.ino`)
El ESP32-C3 no tiene UART2. Cambio ya incluido en `origin/esp32-c3`:

```cpp
#ifdef ESP32C3
SBUS rc(Serial1);   // UART1
#else
SBUS rc(Serial2);   // UART2
#endif
```

> **⚠ Pendiente:** Verificar y configurar los pines RX de Serial1. En ESP32-C3
> los pines por defecto de UART1 son configurables; hay que asignarlos explícitamente
> para que no colisionen con SPI o motores.

### 3. PlatformIO (`platformio.ini`)
- Board: `esp32-c3-devkitm-1`
- Flag `-D ESP32C3` para activar los `#ifdef` del firmware
- `-D ARDUINO_USB_CDC_ON_BOOT=1` necesario para que `Serial` funcione por USB nativo

---

## Trabajo pendiente

### Alta prioridad

- [ ] **Resolver conflicto SPI vs motores**
  Los pines de motores M1/M2/M3 (GPIO 5/6/7) coinciden con los pines SPI por defecto del ESP32-C3.
  Opciones:
  - A) Reasignar el bus SPI del IMU a otros pines en la inicialización de FlixPeriph
  - B) Mover los motores a GPIOs sin conflicto: candidatos `1, 3, 4, 10`
    (evitando GPIO 0 por tener pull-down del MOSFET que puede causar boot en modo download)

- [ ] **Verificar pines SPI del IMU en FlixPeriph**
  `imu.ino` llama a `MPU9250 imu(SPI)` y `imu.begin()` sin especificar pines.
  Confirmar qué CS/SCK/MISO/MOSI asigna FlixPeriph para ESP32-C3 y documentarlos aquí.

- [ ] **Configurar pines de Serial1 para RC**
  Añadir en `rc.ino` o `flix.ino` la asignación explícita del pin RX de Serial1:
  ```cpp
  #ifdef ESP32C3
  Serial1.begin(100000, SERIAL_8E2, PIN_RC_RX, -1, true);
  #endif
  ```
  Definir `PIN_RC_RX` con un GPIO libre (candidato: GPIO 10).

### Media prioridad

- [ ] **LED integrado** (`flix/led.ino`)
  El Super Mini tiene LED en GPIO 8. El firmware ya usa `LED_BUILTIN` con fallback a GPIO 2.
  Verificar si el board `esp32-c3-devkitm-1` define `LED_BUILTIN = 8` o si hay que
  sobreescribirlo en `led.ino`:
  ```cpp
  #ifdef ESP32C3
  #undef LED_BUILTIN
  #define LED_BUILTIN 8
  #endif
  ```

- [ ] **GPIO 9 (botón boot)** como posible entrada de usuario
  El Super Mini tiene el botón BOOT en GPIO 9 accesible. Se podría usar para
  arm/disarm o calibración sin necesitar RC.

- [ ] **Validar `disableBrownOut()`** (`flix.ino`)
  Esta función usa la API de ESP32 Xtensa. Verificar que el equivalente para ESP32-C3
  compila correctamente o añadir `#ifdef`.

### Baja prioridad

- [ ] **Simulator (Gazebo)**
  El simulador está pensado para Ubuntu y no afecta al firmware, pero los
  modelos de física asumen ESP32 original. No es bloqueante.

- [ ] **CI/CD** (`.github/workflows/build.yml`)
  La rama `esp32-c3` del repo original ya añade el build de ESP32-C3 con Arduino CLI.
  Evaluar si migrar el pipeline a PlatformIO o mantener ambos.

---

## Pinout propuesto (borrador — pendiente de validar)

Una vez resuelto el conflicto SPI, el pinout objetivo sería:

| Función | GPIO | Notas |
|---|---|---|
| Motor rear-left (M0) | 1 | Libre, sin conflictos |
| Motor rear-right (M1) | 3 | Libre, sin conflictos |
| Motor front-right (M2) | 4 | Libre, sin conflictos |
| Motor front-left (M3) | 10 | Libre, sin conflictos |
| SPI SCK (IMU) | 6 | Default ESP32-C3 |
| SPI MOSI (IMU) | 7 | Default ESP32-C3 |
| SPI MISO (IMU) | 5 | Default ESP32-C3 |
| SPI CS (IMU) | 2 | A confirmar con FlixPeriph |
| RC SBUS RX (Serial1) | 0 | ⚠ Strapping pin — evaluar alternativa |
| LED integrado | 8 | LED azul del Super Mini |
| USB Serial | 20/21 | UART0, solo para debug |

> Este pinout es provisional. Confirmar con el esquemático final antes de soldar.
