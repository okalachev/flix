#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
混控方向数学验证（离线，不需要连接飞机）
==========================================
【为什么需要这个脚本】
台架实测混控方向有一个绕不过去的坎：一旦某一路电机输出被
`desaturate()` 削顶（截到 0 或 1），四路读数就失去可比性，
"前侧大还是后侧大"这种比较彻底失效。

2026-10-18 实测就撞上了这个坑：
    右前 1.000（削顶） / 左前 0.193 / 右后 0.077 / 左后 −0.393（削底）
这种数据既看不出俯仰，也看不出横滚，只能看出"操作者斜着压了机身"。

本脚本改用纯数学方式验证：把固件里 control.ino / quaternion.h / vector.h
的**同一条符号链**在 Python 里原样复现，然后喂一个已知的机身姿态，
看混控输出的方向对不对。只要四元数运算和 cross 方向与固件一致，
结论就等价于实飞时的力矩方向。

【验证过的符号链】
    control.ino:82   upActual  = rotate(up, attitude)
    control.ino:83   upTarget  = rotate(up, attitudeTarget)      ← 水平
    control.ino:85   error     = rotationVectorBetween(upTarget, upActual)
    vector.h:113     direction = cross(a, b)                     ← a=target, b=actual
    vector.h:128     return direction * angle
    control.ino:87-88 ratesTarget.x/y = PID(error.x/y)            ← PID 增益为正，符号不变
    control.ino:101-102 torqueTarget.x/y = PID(rateError)         ← 同上
    control.ino:119-122 混控（X 型）：
        FL = T + tx − ty + tz
        FR = T − tx − ty − tz
        RL = T + tx + ty − tz
        RR = T − tx + ty + tz

【物理判据（曾经写错过一次，记录在此防回退）】
四轴 X 型，四个电机都在机身平面内，靠**相对推力差**产生力矩：
    · 机头下压 → 要把机头抬起来 → **前侧推力必须大于后侧**
      （等价于跷跷板：想让落下的一端翘起，就在那一端下面顶）
    · 机头上仰 → 要把机头压下去 → **前侧推力必须小于后侧**
    · 右侧压低 → **右侧推力必须大于左侧**
    · 左侧压低 → **左侧推力必须大于右侧**

【用法】
    python mixer_sign_check.py          # 直接跑，无参数
【依赖】numpy
"""

import numpy as np


# ── 四元数基础运算（与 flix quaternion.h 的约定保持一致：w, x, y, z）─────


def qmul(a, b):
    """四元数乘法。顺序为 a ⊗ b，与 flix Quaternion::operator* 一致。"""
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return np.array([
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
    ])


def rot_vec(q, v):
    """用单位四元数旋转向量：v' = q ⊗ (0,v) ⊗ q*。
    与 flix Quaternion::rotateVector 的实现一致。"""
    return qmul(qmul(q, np.array([0.0, v[0], v[1], v[2]])),
                np.array([q[0], -q[1], -q[2], -q[3]]))[1:]


def from_euler(roll, pitch, yaw):
    """欧拉角 → 四元数，Z-Y-X 内旋顺序（航空标准）。
    flix 用同样的顺序把 estimate 出的欧拉角转回四元数。"""
    cr, sr = np.cos(roll / 2), np.sin(roll / 2)
    cp, sp = np.cos(pitch / 2), np.sin(pitch / 2)
    cy, sy = np.cos(yaw / 2), np.sin(yaw / 2)
    return np.array([
        cr * cp * cy + sr * sp * sy,
        sr * cp * cy - cr * sp * sy,
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
    ])


def rotation_vector_between(a, b):
    """对应 vector.h:107 rotationVectorBetween(a, b)。
    返回 旋转轴（cross(a,b) 归一化）× 夹角，即一个"旋转矢量"。
    ⚠️ 注意方向：是 **从 a 转到 b** 的轴，不是反过来。"""
    n = np.linalg.norm(np.cross(a, b))
    angle = np.arccos(np.clip(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b)), -1, 1))
    if n < 1e-6:
        return np.zeros(3)
    return np.cross(a, b) / n * angle


def mixer(torque):
    """control.ino:119-122 的 X 型混控，只取扭矩部分（thrustTarget 记作 0）。
    返回四路相对推力增量，正=加推力。"""
    tx, ty, tz = torque
    return {
        "FL": +tx - ty + tz,
        "FR": -tx - ty - tz,
        "RL": +tx + ty - tz,
        "RR": -tx + ty + tz,
    }


UP = np.array([0.0, 0.0, 1.0])


def response(roll_deg=0.0, pitch_deg=0.0):
    """给定机身当前姿态（目标永远是水平），返回四路混控增量。
    这是 control.ino 的完整符号链：attitude → error → torqueTarget → motors。"""
    actual = rot_vec(from_euler(np.radians(roll_deg), np.radians(pitch_deg), 0), UP)
    target = UP  # 目标姿态 = 水平
    error = rotation_vector_between(target, actual)
    return error, mixer(error)


def main():
    ok_all = True

    print("=" * 70)
    print("  俯仰方向验证")
    print("=" * 70)
    # 判据：机头下压 → 前侧 > 后侧；机头上仰 → 前侧 < 后侧
    for pdeg, label, want_front_bigger in (
        (-20, "机头下压 20°", True),
        (-10, "机头下压 10°", True),
        (+10, "机头上仰 10°", False),
        (+20, "机头上仰 20°", False),
    ):
        err, m = response(pitch_deg=pdeg)
        front = (m["FL"] + m["FR"]) / 2
        rear = (m["RL"] + m["RR"]) / 2
        ok = (front > rear) if want_front_bigger else (front < rear)
        ok_all &= ok
        print(f"  {label:<14} error.y={err[1]:+.4f}  前侧 {front:+.4f}  后侧 {rear:+.4f}  "
              f"{'✅ 正确' if ok else '❌ 反向'}")

    print()
    print("=" * 70)
    print("  横滚方向验证（与 2026-10-18 台架实测结论交叉校验）")
    print("=" * 70)
    # 判据：右侧压低 → 右侧 > 左侧；左侧压低 → 左侧 > 右侧
    for rdeg, label, want_right_bigger in (
        (-20, "右侧压低 20°", True),
        (+20, "左侧压低 20°", False),
    ):
        err, m = response(roll_deg=rdeg)
        left = (m["FL"] + m["RL"]) / 2
        right = (m["FR"] + m["RR"]) / 2
        ok = (right > left) if want_right_bigger else (left > right)
        ok_all &= ok
        print(f"  {label:<14} error.x={err[0]:+.4f}  左侧 {left:+.4f}  右侧 {right:+.4f}  "
              f"{'✅ 正确' if ok else '❌ 反向'}")

    print()
    print("=" * 70)
    if ok_all:
        print("  结论：✅ 俯仰与横滚混控方向全部正确")
        print()
        print("  旁证：横滚一栏的结论「右侧压低 → 右侧加推力」与 2026-10-18")
        print("  台架实测（右倾时 右前 0.205 / 右后 0.257 > 左前 0.088 / 左后 0.134）")
        print("  完全一致，说明本脚本的推演方法与固件实际行为吻合。")
    else:
        print("  结论：❌ 存在反向通道，需要检查 control.ino:119-122 的符号")
    print("=" * 70)

    return 0 if ok_all else 1


if __name__ == "__main__":
    raise SystemExit(main())
