"""生成 README 用的主界面截图。

用法（在项目根目录，先 ./build.sh 编译）：python tools/screenshot.py
输出：
  docs/screenshot.png      英文界面（README.md）
  docs/screenshot_CN.png   中文界面（README_CN.md）

流程：在临时目录生成一份演示数据（2026 年 11 月的事件、固定日程、任务、日记、节假日），
用 build/LookForward.exe --snapshot 把主界面渲染为带 ANSI 颜色的文本，再画成 PNG。
制表符用线条画出（字体里的制表符上下接不上），配色为 Windows Terminal 的 Campbell。

需要 Pillow 和 fontTools（pip install pillow fonttools），以及 Windows 自带的
Consolas、微软雅黑、Segoe UI Symbol、Segoe UI Emoji 字体。
注意：截图中的“今天”是运行时的日期，演示数据在 2026 年 11 月，在其他月份运行时需要相应修改 DEMO_*。
"""

import json
import re
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build" / "LookForward.exe"
SIZE = "150x34"  # 终端列数 x 行数

# ---------------------------------------------------------------- 演示数据


def write(root, rel, obj):
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(obj if isinstance(obj, str) else json.dumps(obj, ensure_ascii=False, indent=2), encoding="utf-8")


def ev(title, start="", end="", note="", remind=None):
    e = {"title": title, "start": start, "end": end, "note": note}
    if remind is not None:
        e["remind"] = remind
    return e


def task(root, fid, tid, name, created, total_h, due, weekdays, progress):
    log = [{"at": created + "T21:00:00", "op": "create", "name": name, "total_minutes": total_h * 60, "due": due,
            "weekdays": weekdays, "skip_holiday": True, "holiday_config": 1}]
    for d, m, note in progress:
        log.append({"at": d + "T22:00:00", "op": "progress", "date": d, "minutes": m, "note": note})
    write(root, f"task/{fid}.json", {"id": tid, "name": name, "log": log})


def make_demo(root, lang):
    cn = lang == "CN"
    T = (lambda c, e: c) if cn else (lambda c, e: e)

    write(root, "preference.json", {"holiday_config": 1, "language": lang, "lunar": True, "slogan": "Look Forward!"})
    write(root, "holidays/config1_2026.json", {
        "country": T("日本", "Japan"), "year": 2026, "days": [
            {"date": "2026-11-03", "name": T("文化之日", "Culture"), "off": True},
            {"date": "2026-11-23", "name": T("勤劳感谢日", "Labor"), "off": True},
        ]})
    write(root, "general/2026/11.json", {
        "2026-11-05": {"events": [ev(T("看牙医", "Dentist"), "10:00", "11:00")]},
        "2026-11-12": {"events": [ev(T("开题汇报", "Proposal talk"), "14:00", "15:30", T("带上打印稿", "bring printouts"))]},
        "2026-11-18": {"events": [ev(T("朋友聚餐", "Dinner with friends"), "19:00")]},
        "2026-11-22": {"events": [ev(T("交周报", "Weekly report due"), "17:00", "", "", 30),
                                  ev(T("看电影", "Movie night"), "20:00", "22:00", T("和室友", "with roommates"))]},
        "2026-11-24": {"events": [ev(T("论文中期检查", "Thesis midterm review"), "10:00", "12:00", "", 3 * 1440)]},
        "2026-11-26": {"events": [ev(T("体检", "Health check"), "09:00")]},
        "2026-11-28": {"events": [ev(T("搬家", "Moving day")), ev(T("还书", "Return library books"), "16:00")]},
    })
    write(root, "fixed/20261001_b71c.json", {
        "id": "b71c", "title": T("组会", "Lab meeting"), "start": "09:00", "end": "10:00", "note": T("会议室 A", "Room A"),
        "repeat": "weekly", "weekdays": [2, 4], "from": "2026-10-01", "until": ""})
    write(root, "fixed/20261003_c2d4.json", {
        "id": "c2d4", "title": T("英语课", "English class"), "start": "10:00", "end": "11:30", "note": "",
        "repeat": "weekly", "weekdays": [7], "from": "2026-10-04", "until": ""})
    task(root, "20261102_a3f9", "a3f9", T("毕业论文", "Thesis"), "2026-11-02", 60, "2026-12-20", [1, 2, 3, 4, 5], [
        ("2026-11-02", 120, ""), ("2026-11-04", 150, T("文献综述", "literature review")),
        ("2026-11-06", 90, ""), ("2026-11-10", 180, T("实验设计", "experiment design")),
        ("2026-11-13", 120, ""), ("2026-11-17", 150, ""), ("2026-11-19", 120, T("跑实验", "ran experiments")),
        ("2026-11-20", 90, "")])
    task(root, "20261110_5e21", "5e21", T("读完《设计模式》", "Read 'Design Patterns'"), "2026-11-10", 15, "2026-12-06",
         [1, 2, 3, 4, 5, 6, 7], [
             ("2026-11-10", 40, ""), ("2026-11-11", 30, ""), ("2026-11-14", 60, T("第 3 章", "chapter 3")),
             ("2026-11-15", 50, ""), ("2026-11-18", 30, ""), ("2026-11-21", 60, "")])
    for d, text in [
        ("2026-11-03", T("放假，去爬了山。", "Day off, went hiking.")),
        ("2026-11-08", T("周末整理了房间。", "Cleaned my room.")),
        ("2026-11-15", T("读书会。", "Book club.")),
        ("2026-11-21", T("实验终于跑通了！", "The experiment finally worked!")),
    ]:
        write(root, f"diary/2026/{d}.txt", text)


# ---------------------------------------------------------------- ANSI 文本 → PNG

CW, CH, FS = 12, 26, 20  # 单元格宽、高，字号
PAD = 16
FONT_DIR = "C:/Windows/Fonts/"


def load_font(name, size):
    return ImageFont.truetype(FONT_DIR + name, size)


FONT, FONT_B = load_font("consola.ttf", FS), load_font("consolab.ttf", FS)
CJK, CJK_B = load_font("msyh.ttc", FS + 1), load_font("msyhbd.ttc", FS + 1)
SYM, EMJ = load_font("seguisym.ttf", FS - 2), load_font("seguiemj.ttf", FS - 4)
# 按字体实际包含的字符选择字体（缺字时 Pillow 会画成方框，不会自动回退）
_CMAPS = {n: TTFont(FONT_DIR + n, fontNumber=0).getBestCmap()
          for n in ("consola.ttf", "msyh.ttc", "seguisym.ttf", "seguiemj.ttf")}
CMAP = {FONT: _CMAPS["consola.ttf"], FONT_B: _CMAPS["consola.ttf"], CJK: _CMAPS["msyh.ttc"],
        CJK_B: _CMAPS["msyh.ttc"], SYM: _CMAPS["seguisym.ttf"], EMJ: _CMAPS["seguiemj.ttf"]}

# Windows Terminal "Campbell" 配色；深蓝在黑底上看不清，稍微调亮
PALETTE = [(12, 12, 12), (197, 15, 31), (19, 161, 14), (193, 156, 0), (88, 130, 255), (136, 23, 152), (58, 150, 221),
           (204, 204, 204), (118, 118, 118), (231, 72, 86), (22, 198, 12), (249, 241, 165), (59, 120, 255),
           (180, 0, 158), (97, 214, 214), (242, 242, 242)]
BG, FG = (12, 12, 12), (204, 204, 204)

# 制表符：上、右、下、左 四个方向的线宽（0 无，1 细，2 粗）
BOX = {"─": (0, 1, 0, 1), "━": (0, 2, 0, 2), "│": (1, 0, 1, 0), "┃": (2, 0, 2, 0), "╹": (2, 0, 0, 0),
       "╻": (0, 0, 2, 0), "┌": (0, 1, 1, 0), "┐": (0, 0, 1, 1), "└": (1, 1, 0, 0), "┘": (1, 0, 0, 1),
       "├": (1, 1, 1, 0), "┤": (1, 0, 1, 1), "┬": (0, 1, 1, 1), "┴": (1, 1, 0, 1), "┼": (1, 1, 1, 1),
       "╭": (0, 1, 1, 0), "╮": (0, 0, 1, 1), "╰": (1, 1, 0, 0), "╯": (1, 0, 0, 1)}


def width(ch):
    return 2 if unicodedata.east_asian_width(ch) in "WF" else 1


def color256(n):
    if n < 16:
        return PALETTE[n]
    if n < 232:
        n -= 16
        return tuple(0 if v == 0 else 55 + v * 40 for v in (n // 36, n // 6 % 6, n % 6))
    v = 8 + (n - 232) * 10
    return (v, v, v)


def new_style():
    return {"fg": None, "bg": None, "bold": False, "dim": False, "inv": False}


def parse(text):
    """返回行列表，每行是 [(字符, 样式)]；宽字符后补 None 占位"""
    st = new_style()
    rows = []
    for line in text.split("\n"):
        row = []
        for tok in re.split(r"(\x1b\[[0-9;]*m)", line):
            if not tok.startswith("\x1b["):
                for ch in tok:
                    row.append((ch, dict(st)))
                    if width(ch) == 2:
                        row.append(None)
                continue
            codes = [int(c) if c else 0 for c in tok[2:-1].split(";")]
            i = 0
            while i < len(codes):
                c = codes[i]
                if c == 0: st = new_style()
                elif c == 1: st["bold"] = True
                elif c == 2: st["dim"] = True
                elif c == 22: st["bold"] = st["dim"] = False
                elif c == 7: st["inv"] = True
                elif c == 27: st["inv"] = False
                elif 30 <= c <= 37: st["fg"] = PALETTE[c - 30]
                elif 90 <= c <= 97: st["fg"] = PALETTE[c - 90 + 8]
                elif 40 <= c <= 47: st["bg"] = PALETTE[c - 40]
                elif 100 <= c <= 107: st["bg"] = PALETTE[c - 100 + 8]
                elif c == 39: st["fg"] = None
                elif c == 49: st["bg"] = None
                elif c in (38, 48):
                    if codes[i + 1] == 5:
                        col = color256(codes[i + 2])
                        i += 2
                    else:
                        col = tuple(codes[i + 2:i + 5])
                        i += 4
                    st["fg" if c == 38 else "bg"] = col
                i += 1
        rows.append(row)
    while rows and not rows[-1]:
        rows.pop()
    return rows


def mix(a, b, t):
    return tuple(int(x * (1 - t) + y * t) for x, y in zip(a, b))


def draw_box(d, x, y, dirs, col):
    cx, cy = x + CW // 2, y + CH // 2
    for k, (dx, dy) in enumerate([(0, -1), (1, 0), (0, 1), (-1, 0)]):
        if dirs[k]:
            ex = x + CW if dx > 0 else x if dx < 0 else cx
            ey = y + CH if dy > 0 else y if dy < 0 else cy
            d.line([(cx, cy), (ex, ey)], fill=col, width=1 if dirs[k] == 1 else 3)


def font_for(ch, bold):
    first = (CJK_B if bold else CJK) if width(ch) == 2 else (FONT_B if bold else FONT)
    for f in (first, SYM, CJK, EMJ):
        if ord(ch) in CMAP[f]:
            return f
    return first


def render(text, out):
    rows = parse(text)
    cols = max(len(r) for r in rows)
    img = Image.new("RGB", (cols * CW + 2 * PAD, len(rows) * CH + 2 * PAD), BG)
    d = ImageDraw.Draw(img)
    for r, row in enumerate(rows):
        for c, cell in enumerate(row):
            if cell is None:
                continue
            ch, st = cell
            fg, bg = st["fg"] or FG, st["bg"] or BG
            if st["inv"]:
                fg, bg = bg, fg
            if st["dim"]:
                fg = mix(fg, bg, 0.45)
            x, y, w = PAD + c * CW, PAD + r * CH, width(ch) * CW
            if bg != BG:
                d.rectangle([x, y, x + w - 1, y + CH - 1], fill=bg)
            if ch == " ":
                continue
            if ch in BOX:
                draw_box(d, x, y, BOX[ch], fg)
                continue
            f = font_for(ch, st["bold"])
            d.text((x + w / 2, y + CH * 0.74), ch, font=f, fill=fg, anchor="ms", embedded_color=f is EMJ)
    img.save(out, optimize=True)


def main():
    if not EXE.exists():
        sys.exit(f"{EXE} 不存在，请先运行 ./build.sh")
    for lang, name in (("EN", "screenshot.png"), ("CN", "screenshot_CN.png")):
        with tempfile.TemporaryDirectory() as tmp:
            make_demo(Path(tmp), lang)
            out = subprocess.run([str(EXE), "--data", tmp, "--snapshot", SIZE], capture_output=True, check=True).stdout
        # Windows 的 stdout 文本模式会把 ToString() 中的 \r\n 再变成 \r\r\n，去掉所有 \r
        render(out.decode("utf-8").replace("\r", ""), ROOT / "docs" / name)
        print(f"已生成 docs/{name}")


if __name__ == "__main__":
    main()
