# -*- coding: utf-8 -*-
r"""
md2html.py —— 把 Docs 下的 Markdown 文档渲染成「自包含单文件 HTML」（与说明书同一套视觉语言）。

【为什么需要它】
    `Docs/flix技术说明书.html` 是手写的图文版，md 由 `html2md.py` **反向**生成；
    但台架清单是**先有 md**（工程师照着勾选，改一行就生效），再需要一份能直接双击打开、
    排版好看的 html 给现场用（手机/平板/打印都方便）。方向相反，所以要这个脚本。

【设计取舍】
    · 只依赖标准库：本机 github.com 与 pypi 都不通，装不了 markdown/pandoc；
      而且清单的语法子集很小（标题 / 表格 / 代码块 / 勾选列表 / 引用提示 / 有序步骤），
      手写一个 200 行的解析器比引入通用库更可控，也不会被通用库的表格怪癖坑到。
    · **md 是唯一源**：数字与措辞只改 md，重跑本脚本生成 html。
      不要直接改 html —— 下次重生成会被覆盖（脚本会在页脚写明这一点）。
    · 视觉对齐说明书：同一套 CSS 变量（品牌红 #c8102e / 深色代码块 / 圆角卡片），
      现场不会觉得是两份文档。
    · 文本先 HTML 转义再做行内标记：清单里有 `<热点名>` `<ssid>` 这类**字面尖括号**
      （用来提示"尖括号是占位符"），不转义会被浏览器当成标签吞掉。

【用法】
    python Firmware/tools/md2html.py Docs/火龙果版台架实测清单V1.1.md
    python Firmware/tools/md2html.py <in.md> <out.html> [--title "自定义标题"]

【支持的语法子集】
    # / ## / ### 标题          | GFM 表格 | 围栏代码块 ```lang
    - 列表 / - [ ] 勾选项      | 1. 有序步骤 | --- 分隔线
    > 引用（🔴→danger / ⚠️→warn / ✅→ok / 其余 note）
    行内：`code`、**粗体**、*斜体*、[文字](链接)
    ⚠️ 不支持：图片、嵌套列表、HTML 直插（清单里都不需要；真要用就写进 md 的代码块）
"""
import io
import re
import sys
import html as html_mod
import os


# ---------------------------------------------------------------------------
# 行内标记：先整体 HTML 转义，再把 `code` / **粗体** / *斜体* / [链接](url) 还原
# ---------------------------------------------------------------------------
def inline(text):
    """把一行 Markdown 行内语法转成 HTML。

    参数：text —— 原始 md 行（未转义）
    返回：HTML 片段（已转义，可直接放入标签内）
    为什么先转义：清单里存在 `<ssid>` `<热点名>` 这类字面尖括号，必须在识别标记之前
    先把 & < > 变成实体，否则会被浏览器当作 HTML 标签吃掉（现场看到的就是空白）。
    代码片段用占位符保护起来，避免 `*args` 之类被误当成斜体。
    """
    text = html_mod.escape(text, quote=False)

    # ① 用占位符保护行内代码（\x00 不会出现在正常文本里）
    codes = []

    def stash(m):
        codes.append(m.group(1))
        return "\x00%d\x00" % (len(codes) - 1)

    text = re.sub(r"`([^`]+)`", stash, text)

    # ② 链接：站内相对链接保留（GitHub/本地都能点），外链加 target=_blank
    def link(m):
        label, url = m.group(1), m.group(2)
        ext = url.startswith("http")
        attr = ' target="_blank" rel="noopener"' if ext else ""
        return '<a href="%s"%s>%s</a>' % (url, attr, label)

    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link, text)

    # ③ 粗体 / 斜体（先 ** 再 *，避免粗体被拆）
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<em>\1</em>", text)

    # ④ 还原行内代码
    for i, c in enumerate(codes):
        text = text.replace("\x00%d\x00" % i, "<code>%s</code>" % c)
    return text


# ---------------------------------------------------------------------------
# CSS：与 Docs/flix技术说明书.html 同一套变量，去掉封面/目录等长篇专用样式
# ---------------------------------------------------------------------------
CSS = """
  :root{
    --brand:#c8102e; --brand-dark:#8f0c22; --ink:#0f172a; --ink2:#334155;
    --ink3:#64748b; --line:#e2e8f0; --bg:#ffffff; --bg2:#f8fafc; --code-bg:#0f172a;
  }
  *{box-sizing:border-box;}
  body{margin:0; background:var(--bg2); color:var(--ink); font-size:15px; line-height:1.85;
       font-family:"Microsoft YaHei","PingFang SC","Hiragino Sans GB","Source Han Sans SC",
                   -apple-system,"Segoe UI",sans-serif; -webkit-font-smoothing:antialiased;}
  .wrap{max-width:1000px; margin:0 auto; padding:0 26px 70px; background:var(--bg);
        box-shadow:0 0 40px rgba(15,23,42,.06);}

  /* 顶部品牌条 */
  .topbar{display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap:wrap;
          margin:0 -26px; padding:12px 26px; background:linear-gradient(90deg,#0f172a,#7f1d1d 130%);
          color:#e2e8f0; font-size:12.5px; letter-spacing:.4px;}
  .topbar b{color:#fff;}
  .topbar .tag{background:rgba(255,255,255,.14); border:1px solid rgba(255,255,255,.22);
               padding:2px 10px; border-radius:12px;}

  h1{font-size:29px; font-weight:800; line-height:1.35; margin:34px 0 6px; color:var(--ink);
     padding-bottom:16px; border-bottom:4px solid var(--brand); letter-spacing:.3px;}
  h2{font-size:22px; font-weight:700; color:var(--brand); margin:46px 0 16px;
     padding-bottom:10px; border-bottom:2px solid var(--brand);}
  h3{font-size:17.5px; font-weight:700; color:var(--ink); margin:30px 0 12px;
     padding-left:12px; border-left:5px solid var(--brand);}
  p{margin:11px 0;}
  hr{border:0; border-top:1px dashed var(--line); margin:34px 0;}

  code{background:#f1f5f9; color:#b91c1c; padding:2px 6px; border-radius:4px; font-size:13.5px;
       font-family:"Cascadia Mono","Consolas","Courier New",monospace;}
  pre{background:var(--code-bg); color:#e2e8f0; padding:16px 20px; border-radius:10px;
      overflow-x:auto; font-size:13px; line-height:1.7; margin:16px 0;
      font-family:"Cascadia Mono","Consolas","Courier New",monospace;}
  pre code{background:none; color:inherit; padding:0; font-size:13px;}

  table{border-collapse:collapse; width:100%; margin:18px 0; font-size:13.8px;
        background:#fff; border:1px solid var(--line); border-radius:8px; overflow:hidden;}
  th,td{border:1px solid var(--line); padding:9px 12px; text-align:left; vertical-align:top;}
  th{background:var(--bg2); color:var(--ink2); font-weight:700; white-space:nowrap;}
  tr:nth-child(even) td{background:#fcfdff;}

  ul,ol{margin:12px 0; padding-left:26px;}
  li{margin:5px 0;}
  ul.check{list-style:none; padding-left:6px;}
  ul.check li{position:relative; padding-left:28px; margin:7px 0;}
  ul.check li .box{position:absolute; left:2px; top:1px; width:16px; height:16px;
        border:1.6px solid var(--ink3); border-radius:3px; display:inline-block;
        text-align:center; line-height:14px; font-size:12px; color:#fff;}
  ul.check li.done .box{background:#16a34a; border-color:#16a34a;}
  ul.check li.done{color:var(--ink3);}

  blockquote{margin:16px 0; padding:13px 18px; border-radius:0 8px 8px 0;
             background:#eff6ff; border-left:5px solid #2563eb; color:var(--ink2);}
  blockquote p{margin:6px 0;}
  blockquote.warn{background:#fff7ed; border-left-color:#ea580c;}
  blockquote.danger{background:#fef2f2; border-left-color:var(--brand);}
  blockquote.ok{background:#f0fdf4; border-left-color:#16a34a;}

  .foot{margin-top:56px; padding-top:18px; border-top:1px solid var(--line);
        font-size:12.5px; color:var(--ink3);}
  .foot a{color:var(--brand); text-decoration:none;}
  @media print{
    body{background:#fff;} .wrap{box-shadow:none; max-width:none;}
    .topbar{background:none; color:var(--ink3);}
    h2{page-break-after:avoid;} pre,table,blockquote{page-break-inside:avoid;}
  }
"""

# 引用块按首个图标决定语义配色（清单里靠图标区分"坑"和"提示"）
QUOTE_CLASS = (("🔴", "danger"), ("⚠️", "warn"), ("✅", "ok"))


def is_block_start(line):
    """判断一行是否是"块级语法"的开头（用于判定列表项的续行在哪里结束）。

    参数：line —— 原始行
    返回：True = 新块开始（标题/表格/引用/代码围栏/列表项/分隔线），False = 上一块的续行
    为什么单独抽出来：列表项与段落的"续行合并"逻辑都要用同一套判据，
    两处各写一份迟早会不一致（这里踩过：只改了一处，粗体仍被截断）。
    """
    s = line.strip()
    if not s:
        return True
    return bool(s.startswith(("|", ">", "#", "```"))
                or re.match(r"^[-*]\s+", s) or re.match(r"^\d+\.\s+", s)
                or re.match(r"^-{3,}$", s))


def convert(md_text, doc_title=None, source_name=None):
    """把整份 Markdown 转成 HTML 字符串。

    参数：md_text     —— md 全文
          doc_title   —— 页面 <title>，缺省取第一个 # 标题
          source_name —— 源文件名（写进页脚，提示"改 md 不要改 html"）
    返回：完整 HTML 文本（自包含）
    调用时机：改完任何清单 md 后手动跑一次（也可挂进发布脚本）。
    """
    lines = md_text.replace("\r\n", "\n").split("\n")
    out = []           # 已生成的 HTML 块
    title = doc_title
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        stripped = line.strip()

        # ---- 空行 ----
        if not stripped:
            i += 1
            continue

        # ---- 围栏代码块 ----
        if stripped.startswith("```"):
            lang = stripped[3:].strip()
            i += 1
            buf = []
            while i < n and not lines[i].strip().startswith("```"):
                buf.append(lines[i])
                i += 1
            i += 1                                   # 跳过结尾 ```
            code = html_mod.escape("\n".join(buf), quote=False)
            cls = ' class="lang-%s"' % lang if lang else ""
            out.append("<pre><code%s>%s</code></pre>" % (cls, code))
            continue

        # ---- 标题 ----
        m = re.match(r"^(#{1,6})\s+(.*)$", stripped)
        if m:
            level, text = len(m.group(1)), m.group(2).strip()
            if level == 1:
                title = title or text
                out.append("<h1>%s</h1>" % inline(text))
            else:
                tag = "h2" if level == 2 else "h3"
                out.append("<%s>%s</%s>" % (tag, inline(text), tag))
            i += 1
            continue

        # ---- 分隔线 ----
        if re.match(r"^-{3,}$", stripped):
            out.append("<hr>")
            i += 1
            continue

        # ---- 表格：连续的 | 开头行 ----
        if stripped.startswith("|"):
            rows = []
            while i < n and lines[i].strip().startswith("|"):
                rows.append(lines[i].strip())
                i += 1
            out.append(render_table(rows))
            continue

        # ---- 引用块：连续的 > 开头行 ----
        if stripped.startswith(">"):
            buf = []
            while i < n and lines[i].strip().startswith(">"):
                buf.append(lines[i].strip()[1:].strip())
                i += 1
            cls = ""
            # 只看**首行**的图标决定配色：清单的说明块常常"开头是目的、下一段才是 ⚠️ 提醒"，
            # 若按整块扫描，一条普通说明会因为中间夹了个 ⚠️ 被染成警示色，现场分不清轻重。
            head = buf[0] if buf else ""
            for icon, name in QUOTE_CLASS:
                if icon in head:
                    cls = " class=\"%s\"" % name
                    break
            body = "".join("<p>%s</p>" % inline(b) for b in buf if b)
            out.append("<blockquote%s>%s</blockquote>" % (cls, body))
            continue

        # ---- 无序列表（含 - [ ] 勾选项）----
        if re.match(r"^[-*]\s+", stripped):
            items, has_check = [], False
            while i < n and re.match(r"^[-*]\s+", lines[i].strip()):
                raw = re.sub(r"^[-*]\s+", "", lines[i].strip())
                i += 1
                # 【为什么要这个内层循环】清单里有大量"一条要点写两行"的情况：
                #     - ❌ 不要用 `Serial.flush()`：……
                #       which **discards the entire TX buffer** ……
                # 续行没有缩进标记，早期版本会把它当成独立段落，导致 **粗体** 被
                # 列表边界截断（页面上出现字面星号）。这里把后续非块起始行并入同一条。
                while i < n and lines[i].strip() and not is_block_start(lines[i]):
                    raw += " " + lines[i].strip()
                    i += 1
                cm = re.match(r"^\[( |x|X)\]\s*(.*)$", raw)
                if cm:
                    has_check = True
                    done = cm.group(1).lower() == "x"
                    items.append('<li class="%s"><span class="box">%s</span>%s</li>'
                                 % ("done" if done else "", "✓" if done else "", inline(cm.group(2))))
                else:
                    items.append("<li>%s</li>" % inline(raw))
            out.append('<ul%s>%s</ul>' % (' class="check"' if has_check else "", "".join(items)))
            continue

        # ---- 有序列表 ----
        if re.match(r"^\d+\.\s+", stripped):
            items = []
            while i < n and re.match(r"^\d+\.\s+", lines[i].strip()):
                raw = re.sub(r"^\d+\.\s+", "", lines[i].strip())
                i += 1
                while i < n and lines[i].strip() and not is_block_start(lines[i]):
                    raw += " " + lines[i].strip()   # 同上：合并续行，避免粗体/代码被截断
                    i += 1
                items.append("<li>%s</li>" % inline(raw))
            out.append("<ol>%s</ol>" % "".join(items))
            continue

        # ---- 普通段落：吃到空行/块起始为止（清单里有不少跨行的长句）----
        buf = []
        while i < n and not is_block_start(lines[i]):
            buf.append(lines[i].strip())
            i += 1
        if buf:
            out.append("<p>%s</p>" % inline(" ".join(buf)))

    title = title or "台架清单"
    foot = ('<div class="foot">本页由 <code>%s</code> 自动生成 —— '
            '要改内容请改 Markdown 源文件后重跑 '
            '<code>python Firmware/tools/md2html.py</code>，直接改 HTML 会被覆盖。<br>'
            '耀坤智联 / YaoKunSmart · <a href="https://github.com/YaoKunS/flix">github.com/YaoKunS/flix</a> · '
            '内核 (C) Oleg Kalachev</div>' % (source_name or "md"))

    return ("<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
            "<title>%s</title>\n<style>%s</style>\n</head>\n<body>\n<div class=\"wrap\">\n"
            "<div class=\"topbar\"><span><b>耀坤智联 / YaoKunSmart</b> · Flix 整理版 v1.1</span>"
            "<span class=\"tag\">台架实测清单 · Bench Checklist</span></div>\n%s\n%s\n</div>\n</body>\n</html>\n"
            % (html_mod.escape(title), CSS, "\n".join(out), foot))


def render_table(rows):
    """把 GFM 表格行渲染成 HTML 表格。

    参数：rows —— 以 '|' 开头/结尾的原始行列表（含第二行的对齐分隔行）
    返回：<table> 片段
    说明：清单表格里存在 `<br />`、行内代码、粗体，所以每个单元格都走 inline()；
         分隔行（第二行，形如 |---|:--:|）只用来判断对齐，不输出。
    """
    def cells(row):
        row = row.strip()
        if row.startswith("|"):
            row = row[1:]
        if row.endswith("|"):
            row = row[:-1]
        return [c.strip() for c in row.split("|")]

    header = cells(rows[0])
    body_rows = rows[2:] if len(rows) > 1 and re.match(r"^[\s:|-]+$", rows[1]) else rows[1:]

    # 对齐：分隔行里 :---: / ---: / :--- 决定列对齐方式
    aligns = []
    if len(rows) > 1 and re.match(r"^[\s:|-]+$", rows[1]):
        for spec in cells(rows[1]):
            if spec.startswith(":") and spec.endswith(":"):
                aligns.append("center")
            elif spec.endswith(":"):
                aligns.append("right")
            else:
                aligns.append("left")

    def attr(idx):
        return ' style="text-align:%s"' % aligns[idx] if idx < len(aligns) and aligns[idx] != "left" else ""

    head_html = "".join("<th%s>%s</th>" % (attr(k), inline(c)) for k, c in enumerate(header))
    body_html = []
    for r in body_rows:
        cs = cells(r)
        # 补齐/截断到表头列数，避免 md 里漏一个竖线就整行错位
        cs = (cs + [""] * len(header))[:len(header)]
        body_html.append("<tr>%s</tr>" % "".join(
            "<td%s>%s</td>" % (attr(k), inline(c)) for k, c in enumerate(cs)))
    return ("<table><thead><tr>%s</tr></thead><tbody>%s</tbody></table>"
            % (head_html, "".join(body_html)))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    title = None
    for a in sys.argv[1:]:
        if a.startswith("--title="):
            title = a.split("=", 1)[1]
    if not args:
        print(__doc__)
        return 1

    src = args[0]
    dst = args[1] if len(args) > 1 else os.path.splitext(src)[0] + ".html"
    md = io.open(src, encoding="utf-8").read()
    html = convert(md, doc_title=title, source_name=os.path.basename(src))
    io.open(dst, "w", encoding="utf-8", newline="\n").write(html)
    print("%s -> %s (%d B)" % (os.path.basename(src), os.path.basename(dst),
                               len(html.encode("utf-8"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
