"""从 src/ui/logo.cpp 中的像素数据生成程序图标和 README 用的图片。

用法（在项目根目录）：python tools/make_icon.py
输出：
  res/LookForward.ico   exe 图标（16/24/32/48/64/128/256，PNG 格式条目）
  docs/logo.png         README 中显示的彩色 logo

只用标准库。修改 logo.cpp 中的 kPixels 或 PixelColor 后重新运行即可。
"""

import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def load_logo():
    src = (ROOT / "src/ui/logo.cpp").read_text(encoding="utf-8")
    body = re.search(r"kPixels = \{(.*?)\};", src, re.S).group(1)
    pixels = re.findall(r'"([^"]*)"', body)
    colors = {
        c: (int(r), int(g), int(b))
        for c, r, g, b in re.findall(r"case '(.)': return Color::RGB\((\d+), (\d+), (\d+)\)", src)
    }
    return pixels, colors


def rgba_rows(pixels, colors, scale, square):
    width = len(pixels[0])
    rows = list(pixels)
    if square:  # 上下补透明行，变成正方形
        pad = width - len(rows)
        rows = ["." * width] * (pad // 2) + rows + ["." * width] * (pad - pad // 2)
    out = []
    for row in rows:
        line = bytearray()
        for c in row:
            r, g, b = colors.get(c, (0, 0, 0))
            line += bytes((r, g, b, 255 if c in colors else 0)) * scale
        out += [bytes(line)] * scale
    return out


def center(rows, size):
    """尺寸不是 logo 宽度的整数倍时（如 24），居中放在透明画布上。"""
    margin = (size - len(rows)) // 2
    if margin <= 0:
        return rows
    left = bytes(4 * margin)
    right = bytes(4 * (size - margin - len(rows)))
    blank = bytes(4 * size)
    return [blank] * margin + [left + r + right for r in rows] + [blank] * (size - margin - len(rows))


def png(rows):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    w, h = len(rows[0]) // 4, len(rows)
    raw = b"".join(b"\x00" + r for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def ico(images):
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, data = b"", b""
    for size, blob in images:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(blob), offset + len(data))
        data += blob
    return header + entries + data


def main():
    pixels, colors = load_logo()
    base = len(pixels[0])
    images = []
    for size in (16, 24, 32, 48, 64, 128, 256):
        scale = max(1, size // base)
        rows = center(rgba_rows(pixels, colors, scale, square=True), size)
        images.append((size, png(rows)))

    (ROOT / "res").mkdir(exist_ok=True)
    (ROOT / "res/LookForward.ico").write_bytes(ico(images))
    (ROOT / "docs/logo.png").write_bytes(png(rgba_rows(pixels, colors, 12, square=False)))
    print("已生成 res/LookForward.ico 和 docs/logo.png")


if __name__ == "__main__":
    main()
