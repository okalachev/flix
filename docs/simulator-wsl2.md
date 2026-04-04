# Correr el simulador en Windows con WSL2

## Primera vez (ya hecho)

Estos pasos solo se hacen una vez:

1. Instalar Ubuntu 20.04 en WSL2:
   ```powershell
   wsl --install -d Ubuntu-20.04
   ```

2. Dentro de Ubuntu 20.04, instalar dependencias:
   ```bash
   sudo sh -c 'echo "deb http://packages.osrfoundation.org/gazebo/ubuntu-stable `lsb_release -cs` main" > /etc/apt/sources.list.d/gazebo-stable.list'
   wget https://packages.osrfoundation.org/gazebo.key -O - | sudo apt-key add -
   sudo apt-get update
   sudo apt-get install -y gazebo11 libgazebo11-dev build-essential libsdl2-dev
   echo "source /usr/share/gazebo/setup.sh" >> ~/.bashrc
   mkdir -p ~/.local/bin
   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
   echo 'export PATH=$PATH:~/.local/bin' >> ~/.bashrc
   source ~/.bashrc
   ```

3. Clonar el repo dentro de Linux (no en /mnt/c):
   ```bash
   cd ~
   git clone https://github.com/sant77/flix.git && cd flix
   git checkout dev/esp32-c3-supermini
   ```

4. Instalar dependencias de Arduino y compilar:
   ```bash
   make dependencies
   make build_simulator
   ```

---

## Cada vez que quieras simular

**1. Abrir Ubuntu 20.04** desde el menú inicio o ejecutar en PowerShell:
```powershell
wsl -d Ubuntu-20.04
```

**2. Ir al repositorio y lanzar:**
```bash
cd ~/flix
make simulator
```

Eso es todo. Gazebo se abre con el drone listo.

---

## Volar en el simulador

En la terminal donde corre el simulador (no una nueva):

```
arm          ← arma los motores
stab         ← activa modo estabilizado
```

Otros comandos útiles:

| Comando | Acción |
|---|---|
| `arm` / `disarm` | Armar / desarmar |
| `stab` | Modo estabilizado (recomendado) |
| `acro` | Modo acrobático (sin estabilización) |
| `ps` | Ver actitud actual (roll, pitch, yaw) |
| `mot` | Ver valores de los 4 motores |
| `imu` | Ver datos del sensor IMU |
| `p` | Listar todos los parámetros PID |
| `p CTL_TILT_MAX 15` | Ejemplo: cambiar un parámetro |

Para control con joystick virtual instala **QGroundControl** en Windows —
se conecta automáticamente al simulador por red local.

---

## Si recompilaste el firmware

Cuando hagas cambios al código hay que recompilar antes de simular:

```bash
make build_simulator
make simulator
```

---

## Apagar WSL cuando termines

```powershell
wsl --shutdown
```
