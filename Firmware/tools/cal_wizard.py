#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
六面加速度计校准向导（人工节奏版）
==================================
【为什么需要这个脚本】
固件里的 `ca` 命令是一条"一口气跑完"的命令：打印第 1/6 步后 `pause(8)`，
8 秒到点立刻采样，接着打印第 2/6 步、再 `pause(8)`…… 全程约 48 秒无任何
停顿等待。如果操作者没能在那 8 秒窗口内把机身翻到指定姿态，六次采样就会
落在同一个姿态上，算出的 accScale 会塌到 1e-2 量级（正常应 ≈1.0），
加速度读数被放大到几十 m/s²，姿态彻底失效。

本脚本的思路：**一次只启动一步**，把"什么时候开始采样"的决定权交给操作者。
  1) 脚本提示当前该摆什么姿态
  2) 操作者摆好、放稳、松手
  3) 按回车 → 脚本才发 `ca`（此时固件自己还会再给 8 秒稳定期）
  4) 脚本回收这一步的结果并打印
然后进入下一步。六步走完自动汇总校验。

【关于固件的 8 秒 pause】
本脚本无法让固件跳过 `pause(8)`，所以每步"按回车 → 采样"之间仍有 8 秒。
这 8 秒恰好当作**额外的稳定等待**，只要按回车时手已离开机身就没问题。
→ 正确节奏：摆好 → 手离开 → 停 2 秒不晃 → 按回车 → 保持不动约 10 秒。

【用法】
    python cal_wizard.py COM28
    python cal_wizard.py COM28 --baud 115200

【依赖】pyserial（已随 Arduino 环境安装）
"""

import sys
import time
import argparse

import serial


# ── 六个面（顺序必须与固件 calibrateAccel() 里 TXT_CAL_1..6 一一对应）──────
# 固件中文提示原文：
#   1/6 水平放置      2/6 机头朝上立起   3/6 机头朝下倒立
#   4/6 右侧朝下侧放  5/6 左侧朝下侧放   6/6 底朝天倒置
# 【为什么顺序是 上下-前后-左右】六个面两两相对，任意相对面给出 ±1g 的一对极值，
# 三对合起来就能同时解出三轴的 bias 与 scale，与具体顺序无关；这里照抄固件顺序。
STEPS = [
    ("1/6", "水平放置", "机腹朝下，四个机臂水平 —— 和正常待机姿态一样"),
    ("2/6", "机头朝上立起", "把机头（机首方向）垂直朝上，尾部着地/着桌"),
    ("3/6", "机头朝下倒立", "把机头垂直朝下，机顶着地"),
    ("4/6", "右侧朝下侧放", "机身向右滚 90°，右侧机臂压在桌面上"),
    ("5/6", "左侧朝下侧放", "机身向左滚 90°，左侧机臂压在桌面上"),
    ("6/6", "底朝天倒置", "整机完全翻转，机顶朝下、机腹朝上"),
]


def open_port(port, baud):
    """打开串口。
    注意：flix 的 print() 走 HWCDC（原生 USB），部分主机上必须显式拉 DTR 才会有输出，
    这里统一在打开后置位 DTR，与 bench_cmd.py 保持一致。"""
    ser = serial.Serial(port, baud, timeout=0.2)
    try:
        ser.dtr = True
    except Exception:
        pass
    return ser


def drain(ser, quiet=0.6, hard_timeout=15.0):
    """把串口里已有的输出读干净。
    结束条件：连续 quiet 秒没有新字节（认为输出停了），或超过 hard_timeout 秒。
    【为什么不用固定 sleep】不同命令输出长度差很多，固定等待要么漏读要么白等。"""
    buf = b""
    start = time.time()
    last = time.time()
    while True:
        chunk = ser.read(4096)
        if chunk:
            buf += chunk
            last = time.time()
        else:
            if buf and (time.time() - last) > quiet:
                break
            if (time.time() - start) > hard_timeout:
                break
    return buf.decode("utf-8", errors="replace")


def send_cmd(ser, cmd, wait_before=0.0):
    """发一条控制台命令并回收输出。
    固件以换行结尾解析命令，这里补 \\n；发完先等一小会儿再读，让固件有时间开始回显。"""
    if wait_before:
        time.sleep(wait_before)
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    ser.flush()
    time.sleep(0.3)
    return drain(ser)


def parse_cal(text):
    """从 `ca` 的完整输出里抽出三个关键数组。
    固件 printIMUCalibration() 的格式：
        陀螺零偏：x y z
        加速度零偏：x y z
        加速度比例系数：x y z
    【为什么返回 None 而不是抛异常】任何一项缺失都说明这次校准没跑完整，
    交给调用方决定是重试还是中止，比直接崩掉更实用。"""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        for key, tag in (
            ("gyro_bias", "陀螺零偏"),
            ("acc_bias", "加速度零偏"),
            ("acc_scale", "加速度比例系数"),
        ):
            if line.startswith(tag):
                body = line.split("：", 1)[-1].split(":", 1)[-1]
                try:
                    out[key] = [float(v) for v in body.split()]
                except ValueError:
                    pass
    if len(out) != 3:
        return None
    return out


def fmt(vec):
    return "  ".join(f"{v:+.4f}" for v in vec)


def main():
    ap = argparse.ArgumentParser(description="六面加速度计校准向导")
    ap.add_argument("port", help="串口，例如 COM28")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    print("=" * 66)
    print("  耀坤S3 六面加速度计校准向导")
    print("=" * 66)
    print()
    print("【重要】每步流程：")
    print("  1) 按下面提示把飞机摆成指定姿态")
    print("  2) 放稳，双手离开机身")
    print("  3) 确认 2 秒内完全不晃 → 按回车")
    print("  4) 回车后保持纹丝不动约 10 秒（固件内部还要再稳 8 秒才采样）")
    print()
    print("⚠️ 姿势摆错或中途晃动，会让这一轮作废（比例系数会明显偏离 1.0）。")
    print()

    ser = open_port(args.port, args.baud)
    print(f"已打开 {args.port}，等待固件空闲…")
    drain(ser, quiet=0.8, hard_timeout=3.0)

    results = []
    for idx, (num, name, detail) in enumerate(STEPS):
        print("-" * 66)
        print(f"  第 {num} 步：{name}")
        print(f"  摆法：{detail}")
        print("-" * 66)
        input(f"  摆好且稳定后，按回车开始采样 >>> ")

        print("  正在采样（约 10 秒，请勿触碰）…")
        t0 = time.time()
        text = send_cmd(ser, "ca", wait_before=0.0)

        # 采样过程固件要跑 8 秒 pause + 1000 次 waitForData，
        # 这里等到出现"校准完成"或超时为止，避免误判提前收尾。
        deadline = time.time() + 25
        while ("校准完成" not in text) and ("Calibration complete" not in text):
            if time.time() > deadline:
                break
            more = drain(ser, quiet=0.8, hard_timeout=4.0)
            if not more:
                break
            text += more

        elapsed = time.time() - t0
        cal = parse_cal(text)
        if cal is None:
            print(f"  ⚠️ 本步未拿到完整校准结果（耗时 {elapsed:.1f}s），输出如下：")
            print("  " + text.replace("\n", "\n  ")[:800])
            results.append(None)
        else:
            print(f"  ✓ 完成（耗时 {elapsed:.1f}s）")
            print(f"    加速度比例系数：{fmt(cal['acc_scale'])}")
            results.append(cal)

        # 每步之间留一点余量，等固件从校准模式回到正常主循环
        time.sleep(1.0)

    ser.close()

    print()
    print("=" * 66)
    print("  校准结果汇总")
    print("=" * 66)
    last = None
    for cal in results:
        if cal is not None:
            last = cal
    if last is None:
        print("❌ 六步都没有拿到有效结果，请检查串口连接后重试。")
        sys.exit(1)

    print(f"  陀螺零偏      ：{fmt(last['gyro_bias'])}   （单位 rad/s，正常量级 <0.05）")
    print(f"  加速度零偏    ：{fmt(last['acc_bias'])}   （单位 m/s²，正常量级 <0.5）")
    print(f"  加速度比例系数：{fmt(last['acc_scale'])}   （应 ≈ 1.000）")
    print()

    # ── 自动判定 ──────────────────────────────────────────────────────────
    # 判据来源：六面校准的数学期望。六个面覆盖每轴 ±1g，理想情况下
    #   accScale = (max-min)/2/G = 1.0，accBias = (max+min)/2 = 0。
    # 0.85~1.15 是留了 ±15% 余量的经验带：超出说明有面没翻到位、
    # 或者某个面采样时手还搭在机身上。
    ok = True
    for axis, (s, b) in zip("XYZ", zip(last["acc_scale"], last["acc_bias"])):
        if not (0.85 <= s <= 1.15):
            print(f"  ⚠️ {axis} 轴比例系数 {s:.4f} 超出 0.85~1.15 —— 该方向的面可能没翻到位")
            ok = False
        if abs(b) > 1.0:
            print(f"  ⚠️ {axis} 轴零偏 {b:.4f} 偏大（>1.0）—— 该方向采样时可能晃动了")
            ok = False

    if ok:
        print("  ✅ 六面校准成功！比例系数与零偏都在正常范围。")
        print()
        print("  下一步验证：把飞机水平静置，执行 `imu`，")
        print("  应看到 横滚≈0°、俯仰≈0°、加速度≈0 0 9.8。")
    else:
        print()
        print("  ❌ 校准未通过。建议重新跑一遍，注意：")
        print("     · 每步按回车之前，手一定要完全离开机身")
        print("     · 六个面要尽量摆到「正」的直角位置，别歪斜")


if __name__ == "__main__":
    main()
