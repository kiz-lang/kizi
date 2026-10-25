#!/usr/bin/env python3
"""kiz 基准测试运行器。

用法:
    python3 scripts/run_benchmarks.py                 # 跑全部基准（每项5轮取中位数）
    python3 scripts/run_benchmarks.py --rounds 7      # 指定轮数
    python3 scripts/run_benchmarks.py --filter dict   # 只跑名字含 dict 的基准
    python3 scripts/run_benchmarks.py --save          # 结果存入 /tmp/kiz_bench_baseline.json
    python3 scripts/run_benchmarks.py --compare       # 与上次 --save 的结果对比（±15%高亮）

说明:
    - 基准用例位于 examples/benchmarks/*.kiz，每个用例自行预热并打印
      "名字 ... 耗时ns" 与 "per_*_ns: 单位耗时" 行。
    - 运行器解析每项的最后一行 per_*_ns（或回退到总耗时行），多轮取中位数
      以抑制系统噪声。
    - --save/--compare 提供跨版本回归对比：改完解释器后先跑一次 --save，
      改完再跑 --compare 即可看到各项变化百分比。
"""

import argparse
import json
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
KIZ = REPO / "build" / "make" / "kiz"
BENCH_DIR = REPO / "examples" / "benchmarks"
DEFAULT_BASELINE = Path("/tmp/kiz_bench_baseline.json")

# 匹配基准输出中的耗时行："名字 N iters: 123456 ns" 或 "per_op_ns: 123 ns"
TOTAL_RE = re.compile(r"(\d+)\s+ns")
PER_RE = re.compile(r"per_\w*ns:\s*(\d+)")


def run_once(path: Path, timeout: int) -> int | None:
    """运行单个基准，返回 per-op 纳秒数（无则返回总耗时）。"""
    proc = subprocess.run(
        [str(KIZ), "run", str(path)],
        capture_output=True, text=True, timeout=timeout,
        stdin=subprocess.DEVNULL,
    )
    if proc.returncode != 0:
        print(f"  [错误] {path.name} 退出码 {proc.returncode}")
        print(proc.stderr[-400:])
        return None
    per = PER_RE.findall(proc.stdout)
    if per:
        return int(per[-1])
    totals = TOTAL_RE.findall(proc.stdout)
    if totals:
        return int(totals[-1])
    print(f"  [警告] {path.name} 输出中未找到耗时行")
    return None


def main() -> None:
    ap = argparse.ArgumentParser(description="kiz 基准测试运行器")
    ap.add_argument("--rounds", type=int, default=5, help="每项运行轮数（默认5，取中位数）")
    ap.add_argument("--filter", default="", help="只运行名字包含该子串的基准")
    ap.add_argument("--save", action="store_true", help="把结果存为基线供 --compare 使用")
    ap.add_argument("--compare", action="store_true", help="与基线对比")
    ap.add_argument("--timeout", type=int, default=300, help="单个用例超时秒数")
    args = ap.parse_args()

    if not KIZ.exists():
        sys.exit(f"[错误] 未找到解释器 {KIZ}，请先构建（make 或 xmake）")

    benches = sorted(p for p in BENCH_DIR.glob("*.kiz") if args.filter in p.name)
    if not benches:
        sys.exit(f"[错误] benchmarks 目录中没有匹配的用例: {BENCH_DIR}")

    print(f"运行 {len(benches)} 项基准 x {args.rounds} 轮（取中位数）\n")
    results: dict[str, int] = {}
    for path in benches:
        samples = []
        for _ in range(args.rounds):
            ns = run_once(path, args.timeout)
            if ns is None:
                break
            samples.append(ns)
        if not samples:
            results[path.stem] = -1
            continue
        med = int(statistics.median(samples))
        results[path.stem] = med
        spread = (max(samples) - min(samples)) / med * 100 if med else 0
        print(f"  {path.stem:<32} {med:>12,} ns/单位   (波动 {spread:.1f}%)")

    print()
    if args.save:
        DEFAULT_BASELINE.write_text(json.dumps(results, indent=2), encoding="utf-8")
        print(f"基线已保存: {DEFAULT_BASELINE}")

    if args.compare:
        if not DEFAULT_BASELINE.exists():
            sys.exit(f"[错误] 没有基线文件 {DEFAULT_BASELINE}，先用 --save 生成")
        base = json.loads(DEFAULT_BASELINE.read_text())
        print(f"{'基准':<32} {'基线':>14} {'当前':>14} {'变化':>9}")
        worse = []
        for name, cur in results.items():
            if name not in base or base[name] <= 0 or cur <= 0:
                continue
            delta = (cur - base[name]) / base[name] * 100
            flag = ""
            if delta > 15:
                flag = "  <-- 回退"
                worse.append(name)
            elif delta < -15:
                flag = "  <-- 改进"
            print(f"  {name:<32} {base[name]:>14,} {cur:>14,} {delta:>+8.1f}%{flag}")
        if worse:
            print(f"\n[注意] {len(worse)} 项性能回退超过15%: {', '.join(worse)}")
        else:
            print("\n无超过15%的回退。")


if __name__ == "__main__":
    main()
