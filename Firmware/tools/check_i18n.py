#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
耀坤整理版 Flix —— 界面中文完整性自检脚本
================================================================================

用途
----
用户要求"耀坤整理版界面全中文"。人工逐行 review 30 多个 print() 必然漏，
所以把这条要求做成**可执行的检查**：

    检查 1  i18n.h 中英文两段的 TXT_* 宏是否**逐条配对**
    检查 2  所有源文件引用的 TXT_* 宏是否都在 i18n.h 中定义
    检查 3  是否存在"绕过 i18n 直接写英文字符串"的 print() 调用
    检查 4  i18n.h 是否为 **UTF-8 无 BOM**（BOM 会让 gcc 报错/乱码）
    检查 5  i18n.h 中是否含非法控制字符

用法
----
    python tools/check_i18n.py                # 在 Firmware/flix 下运行
    python tools/check_i18n.py --verbose      # 列出每一条残留

退出码
------
    0 = 全部通过
    1 = 发现问题（CI 可据此阻断）

白名单说明
----------
以下情形**不算**英文残留，脚本内置白名单放行：

  · 纯格式化串          print("%s\\n", motd)
  · 纯数字/符号格式化    print("%u ", channels[i])
  · 表头式对齐格式串     print("%-15s  %-13g\\n", ...)
  · CSV 列名与数据       log.ino 的输出必须保持英文，否则地面站/Excel 对不上
  · 引脚名/总线名        "SPI" / "I2C" / "MPU6050" 这类专有名词不翻译
================================================================================
"""

import io
import os
import re
import sys
import glob

# 允许保留的"非界面文字"字符串（正则）：纯格式串、CSV、专有名词
# 【白名单思路】先"掏空"所有 %xx 格式说明符，如果剩下的只有空白/标点，
# 就说明这个字面量不含任何自然语言，无需翻译。这样比逐条列举格式串稳得多。
FORMAT_SPEC = re.compile(r'%[-+ 0#]*[0-9]*(?:\.[0-9]+)?(?:hh|h|ll|l|L|z|j|t)?[diufgscxXeEop%]')

ALLOW_PATTERNS = [
    r'^SPI$|^I2C$|^I2C0$',                     # 总线专有名词
    r'^flix$',                                 # 发现包内容（协议固定，不能翻译）
]

# 文件级白名单：整个文件不参与"英文残留"检查
FILE_WHITELIST = {
    'log.ino',      # CSV 输出，列名必须英文
}


def is_pure_format(literal: str) -> bool:
    """
    判断字符串字面量是否为"纯格式串"（去掉 % 说明符后只剩空白与标点）。

    【为什么需要这个函数】控制台里有大量
        print("%-15s  %-13g\\n", name, value)
    这类对齐用格式串。它们不含任何自然语言，翻译它们毫无意义、
    还会破坏列宽对齐。用"掏空格式符再看剩余"的规则可以一次性覆盖所有变体，
    不必逐条手写白名单（那样每加一处对齐格式就得改脚本）。
    """
    stripped = FORMAT_SPEC.sub('', literal)
    # 源码里拿到的是**转义后的**文本（"\n" 是两个字符），先还原成真实字符
    try:
        stripped = stripped.encode('utf-8').decode('unicode_escape')
    except UnicodeDecodeError:
        pass
    # 剩余只允许空白与常见标点
    return re.fullmatch(r'[\s\-+=.:,/\[\]()<>|*#]*', stripped) is not None


def strip_bom(data: bytes):
    """检测并剥离 UTF-8 BOM。返回 (text, had_bom)。"""
    had = data.startswith(b'\xef\xbb\xbf')
    if had:
        data = data[3:]
    return data.decode('utf-8', errors='replace'), had


def check():
    here = os.path.dirname(os.path.abspath(__file__))
    sketch = os.path.normpath(os.path.join(here, '..', 'flix'))
    if not os.path.isdir(sketch):
        print("找不到固件目录：%s" % sketch)
        return 1

    verbose = '--verbose' in sys.argv
    problems = []

    # ---------- 检查 4/5：i18n.h 编码 ----------
    i18n_path = os.path.join(sketch, 'i18n.h')
    raw = open(i18n_path, 'rb').read()
    text, had_bom = strip_bom(raw)
    if had_bom:
        problems.append("i18n.h 带有 UTF-8 BOM（必须去掉，否则编译/串口可能乱码）")
    bad_ctrl = [c for c in text if ord(c) < 32 and c not in '\r\n\t']
    if bad_ctrl:
        problems.append("i18n.h 含非法控制字符 %d 个" % len(bad_ctrl))

    # ---------- 检查 1：中英文段配对 ----------
    idx = text.find('#else')
    if idx < 0:
        problems.append("i18n.h 未找到 '#else' 分隔（中英两段结构缺失）")
        en = zh = set()
    else:
        en = set(re.findall(r'#define\s+(TXT_\w+)', text[:idx]))
        zh = set(re.findall(r'#define\s+(TXT_\w+)', text[idx:]))
        only_en = sorted(en - zh)
        only_zh = sorted(zh - en)
        if only_en:
            problems.append("仅英文段有定义（中文段缺失 %d 条）：%s" % (len(only_en), ', '.join(only_en)))
        if only_zh:
            problems.append("仅中文段有定义（英文段缺失 %d 条）：%s" % (len(only_zh), ', '.join(only_zh)))

    defined = en | zh

    # ---------- 检查 2/3：源文件 ----------
    sources = sorted(glob.glob(os.path.join(sketch, '*.ino')) +
                     glob.glob(os.path.join(sketch, '*.h')))
    referenced = {}
    for path in sources:
        name = os.path.basename(path)
        if name == 'i18n.h':
            continue
        src = io.open(path, encoding='utf-8', errors='replace').read()

        # 检查 2：引用了但没定义
        for macro in set(re.findall(r'\bTXT_\w+', src)):
            referenced.setdefault(macro, set()).add(name)
        for macro in set(re.findall(r'\bTXT_\w+', src)):
            if macro not in defined:
                problems.append("%s 引用了未定义的宏 %s" % (name, macro))

        # 检查 3：绕过 i18n 的字符串字面量
        if name in FILE_WHITELIST:
            continue
        for m in re.finditer(r'\bprint\s*\(\s*"((?:[^"\\]|\\.)*)"', src):
            literal = m.group(1)
            if re.search(r'[\u4e00-\u9fff]', literal):
                continue                # 含中文：已本地化
            if is_pure_format(literal):
                continue                # 纯格式串：无需翻译
            if any(re.match(p, literal) for p in ALLOW_PATTERNS):
                continue                # 专有名词
            line = src[:m.start()].count('\n') + 1
            problems.append("%s:%d 绕过 i18n 的英文字符串：\"%s\"" % (name, line, literal))

    # ---------- 汇总 ----------
    print("=" * 66)
    print("耀坤整理版 Flix —— 界面中文完整性自检")
    print("=" * 66)
    print("固件目录        : %s" % sketch)
    print("英文段宏数      : %d" % len(en))
    print("中文段宏数      : %d" % len(zh))
    print("被引用的宏数    : %d" % len(referenced))
    unused = sorted(defined - set(referenced))
    print("定义但未引用    : %d%s" % (len(unused), ("  → " + ', '.join(unused)) if unused else ""))
    print("-" * 66)

    if problems:
        print("发现 %d 个问题：" % len(problems))
        for p in problems:
            print("  ✗ %s" % p)
        print("=" * 66)
        return 1

    print("✓ 全部通过：中英宏配对、无未定义引用、无绕过 i18n 的英文残留")
    print("=" * 66)
    return 0


if __name__ == '__main__':
    sys.exit(check())
