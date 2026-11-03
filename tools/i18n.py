"""界面文本的提取与检查。

用法（在项目根目录）：
  python tools/i18n.py          扫描 src/ 生成 language/EN.json，并检查其他语言文件
  python tools/i18n.py --check  只检查，不写文件；EN.json 过期或其他语言缺少 key 时返回 1

源码中 Tr("...") 和 N_("...") 的第一个字符串参数就是 key（英文原文）。
EN.json 的 key 与 value 相同，按源码中首次出现的顺序排列，作为翻译模板；
新增语言时复制 EN.json，改名为 <名称>.json（如 JA.json），逐条翻译 value 即可。
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LANG_DIR = ROOT / "language"

# Tr( / N_( 后面紧跟的普通字符串或原始字符串
CALL = re.compile(r'\b(?:Tr|N_)\(\s*(?:R"(?P<delim>[^(\s]*)\((?P<raw>.*?)\)(?P=delim)"|"(?P<str>(?:[^"\\]|\\.)*)")', re.S)
ESCAPES = {"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}


def unescape(s):
    return re.sub(r"\\(.)", lambda m: ESCAPES.get(m.group(1), m.group(0)), s)


def extract_keys():
    keys = []
    seen = set()
    files = sorted(p for p in (ROOT / "src").rglob("*") if p.suffix in (".cpp", ".h"))
    for p in files:
        # 跳过整行注释（注释里的 Tr(...) 示例不是 key）
        lines = p.read_text(encoding="utf-8").splitlines()
        text = "\n".join(line for line in lines if not line.lstrip().startswith("//"))
        for m in CALL.finditer(text):
            key = m.group("raw") if m.group("raw") is not None else unescape(m.group("str"))
            if key not in seen:
                seen.add(key)
                keys.append(key)
    return keys


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=2) + "\n"


def main():
    check_only = "--check" in sys.argv[1:]
    sys.stdout.reconfigure(encoding="utf-8")
    keys = extract_keys()
    template = dump({k: k for k in keys})
    en_path = LANG_DIR / "EN.json"
    ok = True

    if not en_path.exists() or en_path.read_text(encoding="utf-8") != template:
        if check_only:
            print("EN.json 已过期，请运行 python tools/i18n.py")
            ok = False
        else:
            LANG_DIR.mkdir(exist_ok=True)
            en_path.write_text(template, encoding="utf-8")
            print(f"已生成 EN.json（{len(keys)} 条）")

    key_set = set(keys)
    for p in sorted(LANG_DIR.glob("*.json")):
        if p.name == "EN.json":
            continue
        data = json.loads(p.read_text(encoding="utf-8"))
        missing = [k for k in keys if k not in data]
        unused = [k for k in data if k not in key_set]
        print(f"{p.name}：{len(data)} 条，缺少 {len(missing)} 条，多余 {len(unused)} 条")
        for k in missing:
            print("  缺少：", json.dumps(k, ensure_ascii=False))
        for k in unused:
            print("  多余：", json.dumps(k, ensure_ascii=False))
        ok = ok and not missing

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
