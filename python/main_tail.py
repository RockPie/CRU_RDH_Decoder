#!/usr/bin/env python3
import argparse
import mmap
import os
import sys
import time
from pprint import pprint

# 便于本地开发：把 build/python 放到 sys.path 前面
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "build", "python"))

try:
    import pybinparse as m
except Exception as e:
    raise SystemExit(
        f"Failed to import pybinparse: {e}\n"
        "Make sure you built it (e.g. in build/python or installed), "
        "and that the Python version matches the built extension."
    )

LINE_SIZE = 32  # 每行 40 字节

def main():
    parser = argparse.ArgumentParser(
        description="Test pybinparse.count_types_v3 / parse_line on a binary log file."
    )
    parser.add_argument(
        "path",
        nargs="?",
        default="data/2025_09_25_cru_dma_log_example",
        help="Path to the binary file (default: data/2025_09_25_cru_dma_log_example)",
    )
    parser.add_argument(
        "--preview", "-n",
        type=int, default=3,
        help="Preview first N lines with parsed fields (default: 3)"
    )
    args = parser.parse_args()
    path = args.path

    if not os.path.exists(path):
        raise SystemExit(f"File not found: {os.path.abspath(path)}")

    size = os.path.getsize(path)
    if size == 0:
        raise SystemExit("File is empty.")

    total_lines = size // LINE_SIZE

    print(f"Parsing file: {os.path.abspath(path)}")
    print(f"File size: {size} bytes ({total_lines} lines @ {LINE_SIZE}B/line)")

    t0 = time.perf_counter()
    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        stats = m.count_types_v3(mm)  # 新版接口，返回 dict
    t1 = time.perf_counter()

    elapsed = t1 - t0
    mib_read = size / (1024 * 1024)
    mibps = mib_read / elapsed if elapsed > 0 else float("inf")

    print("\n=== Result (from pybinparse.count_types_v3) ===")
    pprint(dict(stats))

    print("\n=== Timing ===")
    print(f"Elapsed time    : {elapsed:.6f} s")
    print(f"Throughput      : {mibps:.2f} MiB/s")

    # 预览前 N 行的解析内容
    n_preview = max(0, min(args.preview, total_lines))
    if n_preview > 0:
        print(f"\n=== Preview first {n_preview} lines ===")
        with open(path, "rb") as f:
            for i in range(n_preview):
                line = f.read(LINE_SIZE)
                if not line:
                    break
                typ, info = m.parse_line(line)  # 返回 (type_str, dict)
                print(f"[{i+1:03d}] Type={typ}")
                if info:
                    for k, v in info.items():
                        print(f"    {k:<20}: {v}")
                else:
                    print("    (no detail)")

if __name__ == "__main__":
    main()