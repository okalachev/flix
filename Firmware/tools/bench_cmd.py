# -*- coding: utf-8 -*-
"""
台架命令执行脚本：发一条控制台命令并回收响应
用法：python bench_cmd.py COM28 "imu" [等待秒]
"""
import sys, time
import serial

port = sys.argv[1]
cmd = sys.argv[2]
wait = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0

ser = serial.Serial(port, 115200, timeout=0.2)
ser.dtr = True
ser.rts = False
time.sleep(0.3)
ser.reset_input_buffer()          # 丢掉开机残留
ser.write((cmd + "\r\n").encode())
ser.flush()

end = time.time() + wait
buf = b""
idle = 0
while time.time() < end:
    chunk = ser.read(4096)
    if chunk:
        buf += chunk
        idle = 0
    else:
        idle += 1
        if idle > 10:             # 约 2 秒无新数据即认为响应结束
            break
ser.close()
sys.stdout.buffer.write(buf)
sys.stdout.buffer.flush()
