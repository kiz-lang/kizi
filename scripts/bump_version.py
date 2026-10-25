#!/usr/bin/env python3
"""kiz 版本号更新脚本——单一入口，替代手动改多处。

用法:
    python3 scripts/bump_version.py 0.10.0      # 指定完整版本号
    python3 scripts/bump_version.py --major     # 1.2.3 -> 2.0.0
    python3 scripts/bump_version.py --minor     # 1.2.3 -> 1.3.0
    python3 scripts/bump_version.py --patch     # 1.2.3 -> 1.2.4

它做的事:
    1. 更新 VERSION 文件（唯一事实源）
    2. 更新 src/version.hpp（仓库内真实头文件，所有构建系统与mcpp直接包含）
    3. 更新 mcpp.toml 的 [package] version
    4. 更新 README.md 中的 "kiz vX.Y.Z" 标题
    5. 扫描仓库中残留的硬编码版本号并提醒（不自动改）

版本来源说明:
    VERSION 文件是唯一事实源。Makefile、CMake、xmake、mcpp 四个构建系统
    都不硬编码版本号：Make 生成 version.hpp（从VERSION），CMake 读取 VERSION，
    xmake 借 $(shell) 读取 VERSION，mcpp 的版本由本脚本同步进 mcpp.toml，
    头文件 src/version.hpp 也由本脚本同步重写。
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = REPO_ROOT / "VERSION"
VERSION_HPP = REPO_ROOT / "src" / "version.hpp"
MCPP_TOML = REPO_ROOT / "mcpp.toml"
README_FILE = REPO_ROOT / "README.md"

# 语义化版本正则：X.Y.Z（纯数字，不含前缀）
SEMVER_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
# README 标题里的版本引用："kiz v0.9.0"
README_VER_RE = re.compile(r"(kiz\s+v)(\d+\.\d+\.\d+)")


def read_version() -> str:
    text = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not SEMVER_RE.match(text):
        sys.exit(f"[错误] VERSION 文件内容不是合法的 X.Y.Z 版本号: {text!r}")
    return text


def bump(cur: str, part: str) -> str:
    major, minor, patch = (int(x) for x in cur.split("."))
    if part == "major":
        return f"{major + 1}.0.0"
    if part == "minor":
        return f"{major}.{minor + 1}.0"
    return f"{major}.{minor}.{patch + 1}"


def update_version_hpp(new: str) -> bool:
    if not VERSION_HPP.exists():
        return False
    major, minor, patch = new.split(".")
    block = (
        "/**\n"
        " * @file version.hpp\n"
        " * @brief 版本号宏定义（由 scripts/bump_version.py 从 VERSION 文件同步更新）\n"
        " * @author azhz1107cat\n"
        " * @date 2026-10-25\n"
        " */\n"
        "\n"
        "#pragma once\n"
        "\n"
        f"#define KIZ_VERSION_MAJOR {major}\n"
        f"#define KIZ_VERSION_MINOR {minor}\n"
        f"#define KIZ_VERSION_PATCH {patch}\n"
        f'#define KIZ_VERSION "{new}"\n'
    )
    VERSION_HPP.write_text(block, encoding="utf-8")
    return True


def update_mcpp_toml(new: str) -> bool:
    if not MCPP_TOML.exists():
        return False
    text = MCPP_TOML.read_text(encoding="utf-8")
    new_text, count = re.subn(
        r'(?m)^(version\s*=\s*)"[^"]*"', lambda m: m.group(1) + f'"{new}"', text, count=1)
    if count:
        MCPP_TOML.write_text(new_text, encoding="utf-8")
    return bool(count)


def update_readme(new: str) -> bool:
    if not README_FILE.exists():
        return False
    text = README_FILE.read_text(encoding="utf-8")
    new_text, count = README_VER_RE.subn(lambda m: m.group(1) + new, text)
    if count:
        README_FILE.write_text(new_text, encoding="utf-8")
    return bool(count)


def scan_stray_versions(old: str, new: str) -> list[str]:
    """扫描可能残留的硬编码旧版本号（只报告，不修改）。"""
    hits = []
    patterns = [re.compile(r"\b" + re.escape(old) + r"\b")]
    skip_dirs = {"build", "cmake-build-debug", ".git", ".xmake", ".idea", "private"}
    for path in REPO_ROOT.rglob("*"):
        if not path.is_file():
            continue
        if any(part in skip_dirs for part in path.parts):
            continue
        if path.suffix not in {".md", ".lua", ".txt", ".cpp", ".hpp", ".in", ".kiz"}:
            continue
        if path == VERSION_FILE or path == Path(__file__):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for pat in patterns:
            for m in pat.finditer(text):
                line_no = text.count("\n", 0, m.start()) + 1
                hits.append(f"{path.relative_to(REPO_ROOT)}:{line_no} 出现旧版本号 {old}")
                break
    return hits


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)

    old = read_version()

    if args[0].startswith("--"):
        part = args[0].lstrip("-")
        if part not in {"major", "minor", "patch"}:
            sys.exit(f"[错误] 未知选项: {args[0]}（可选 --major/--minor/--patch 或直接给版本号）")
        new = bump(old, part)
    else:
        new = args[0].strip().lstrip("v")
        if not SEMVER_RE.match(new):
            sys.exit(f"[错误] 版本号必须是 X.Y.Z 形式（如 0.10.0）: {args[0]!r}")

    if new == old:
        sys.exit(f"[提示] 版本号已是 {old}，无需更新")

    # 1. 写 VERSION（事实源）
    VERSION_FILE.write_text(new + "\n", encoding="utf-8")

    # 2. 同步 src/version.hpp
    hpp_updated = update_version_hpp(new)

    # 3. 同步 mcpp.toml
    mcpp_updated = update_mcpp_toml(new)

    # 4. 更新 README 标题
    readme_updated = update_readme(new)

    # 3. 扫描残留
    stray = scan_stray_versions(old, new)

    print(f"版本号: {old} -> {new}")
    print(f"  [更新] VERSION")
    if hpp_updated:
        print(f"  [更新] src/version.hpp")
    if mcpp_updated:
        print(f"  [更新] mcpp.toml")
    if readme_updated:
        print(f"  [更新] README.md (kiz v{new})")
    else:
        print(f"  [跳过] README.md 中未找到 'kiz vX.Y.Z' 标题")
    if stray:
        print("  [注意] 以下位置仍出现旧版本号，请确认是否需要手工更新：")
        for hit in stray:
            print(f"         {hit}")
    print()
    print("完成。版本号事实源已更新为", new)
    print("各构建系统获取新版本号的方式：")
    print("  make   : 直接 make 即可（自动读取VERSION）")
    print("  CMake  : 重新配置后构建（cmake -B build && cmake --build build）")
    print("  xmake  : 需重新配置（xmake f -c && xmake），configvar在配置期快照")


if __name__ == "__main__":
    main()
