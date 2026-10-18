# -*- coding: utf-8 -*-
"""
I2C 引脚组合自动试探（2 号机 IMU 识别失败诊断用）
对每个 (SDA,SCL) 候选组合：
  1) p IMU_PIN_SDA / IMU_PIN_SCL 写参数（自动存 NVS）
  2) reboot 重启
  3) 等 app 起来后发 imu，看是否识别成功（状态：正常 / WHO != 0x00）
全部试完后把参数恢复为默认 9/8。
用法：python bench_i2c_scan.py COM24
"""
import sys, time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM24"

# 候选组合：ESP32-S3 常见 I2C 布线（默认 9/8 已验证失败，放在最后再试一次反接）
CANDIDATES = [
    (8, 9), (5, 6), (6, 5), (4, 5), (1, 2), (2, 1),
    (41, 42), (42, 41), (47, 48), (12, 13), (17, 18), (35, 36), (10, 11),
]

def open_port():
    ser = serial.Serial(PORT, 115200, timeout=0.2)
    ser.dtr = True; ser.rts = False
    time.sleep(0.3)
    ser.reset_input_buffer()
    return ser

def cmd(ser, c, wait=2.0, idle_limit=8):
    ser.write((c + "\r\n").encode()); ser.flush()
    buf = b""; idle = 0; end = time.time() + wait
    while time.time() < end:
        chunk = ser.read(4096)
        if chunk: buf += chunk; idle = 0
        else:
            idle += 1
            if idle > idle_limit: break
    return buf.decode("utf-8", "replace")

def reboot_and_wait(ser):
    """发 reboot，等 app 重新起来（轮询发 help 直到有响应）"""
    ser.write(b"reboot\r\n"); ser.flush()
    time.sleep(1.0)
    ser.close()
    time.sleep(2.0)                       # 等复位 + IMU 自动识别（含 SPI 探测，约 2~4s）
    for _ in range(20):
        try:
            ser = open_port()
        except Exception:
            time.sleep(0.3); continue
        r = cmd(ser, "help", wait=1.5, idle_limit=5)
        if "命令" in r or "help" in r or "耀坤" in r:
            return ser
        ser.close(); time.sleep(0.5)
    return None

ser = open_port()
results = []
try:
    for sda, scl in CANDIDATES:
        cmd(ser, "p IMU_PIN_SDA %d" % sda, wait=1.5)
        cmd(ser, "p IMU_PIN_SCL %d" % scl, wait=1.5)
        ser = reboot_and_wait(ser)
        if ser is None:
            print("SDA=%d SCL=%d → 复位后失联，中止" % (sda, scl)); break
        r = cmd(ser, "imu", wait=3.0)
        line = " ".join(r.split())[:150]
        ok = ("状态：正常" in r) and ("WHO_AM_I）：0x00" not in r)
        print("SDA=%d SCL=%d → %s" % (sda, scl, "✓ 成功" if ok else "✗"))
        print("    %s" % line)
        results.append(((sda, scl), ok, r))
        if ok:
            break
finally:
    # 无论成败，把引脚参数恢复默认（9/8），避免留脏参数
    try:
        cmd(ser, "p IMU_PIN_SDA 9", wait=1.2)
        cmd(ser, "p IMU_PIN_SCL 8", wait=1.2)
        ser.close()
    except Exception:
        pass
print("==== 试探结束，参数已恢复 9/8 ====")
