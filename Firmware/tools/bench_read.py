# -*- coding: utf-8 -*-
"""
台架串口采集脚本（耀坤整理版 Flix 实测用）
用法：python bench_read.py COM28 [秒数]
- 打开原生 USB-CDC 口（115200，DTR/RTS 置位，否则 HWCDC 不输出）
- 先拉 DTR 触发一次输出，再持续读取指定秒数
- 输出原始文本到 stdout（UTF-8）
"""
import sys, time
import serial

port = sys.argv[1]
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0

ser = serial.Serial(port, 115200, timeout=0.2)
ser.dtr = True   # HWCDC 必须拉 DTR 才有输出（实测约定）
ser.rts = False
# 触发一下（发个回车，让 motd 重新打印也可作为二次确认）
time.sleep(0.3)
ser.write(b"\r\n")
ser.flush()

end = time.time() + secs
buf = b""
while time.time() < end:
    chunk = ser.read(4096)
    if chunk:
        buf += chunk
ser.close()
sys.stdout.buffer.write(buf)
sys.stdout.buffer.flush()
