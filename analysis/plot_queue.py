#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import glob
import argparse
import matplotlib.pyplot as plt

def find_cdf_files(root="."):
    # 递归找两类 CDF 文件
    voq_files = sorted(glob.glob(os.path.join(root, "**", "*_out_voq_cdf.txt"), recursive=True))
    per_dst_files = sorted(glob.glob(os.path.join(root, "**", "*_out_voq_per_dst_cdf.txt"), recursive=True))
    return voq_files, per_dst_files

def read_cdf_file(path):
    """文件每行形如: <value> <cnt> <acc_cnt> <prob>。我们取 value 与 prob。"""
    xs, ys = [], []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            # 兼容意外行：至少要有4列
            if len(parts) < 4:
                continue
            try:
                x = float(parts[0])
                y = float(parts[3])
            except ValueError:
                continue
            xs.append(x)
            ys.append(y)
    return xs, ys

def label_from_path(path):
    """自动生成曲线标签：优先用 run id（路径中的数字文件夹），否则用文件名。"""
    base = os.path.basename(path)
    # 例如 .../mix/output/953111981/953111981_out_voq_cdf.txt
    # 抓取第一个连续的数字段作为 id
    import re
    m = re.search(r"/(\d{3,})/", path.replace("\\","/"))
    if m:
        return m.group(1)
    # 或者从文件名前缀获取
    m2 = re.match(r"(\d{3,})_", base)
    if m2:
        return m2.group(1)
    return base

def plot_many(series, title, xlab, ylab, outfile):
    if not series:
        print(f"[WARN] 没有可画的数据：{title}")
        return
    os.makedirs(os.path.dirname(outfile), exist_ok=True)

    plt.figure(figsize=(5, 4))
    for label, (xs, ys) in series.items():
        if not xs or not ys:
            print(f"[WARN] 曲线 {label} 为空，跳过。")
            continue
        # 画线：不指定颜色/样式，Matplotlib 会自动区分
        plt.plot(xs, ys, linewidth=2, label=label)

    plt.xlabel(xlab)
    plt.ylabel(ylab)
    plt.title(title)
    plt.grid(True, which="both", alpha=0.4)
    # 把图例放到图外上方靠右，避免遮挡曲线
    plt.legend(loc="upper left", bbox_to_anchor=(0.0, 1.15), ncol=3, frameon=False)  # 位置可根据需要微调
    plt.tight_layout()
    plt.savefig(outfile, bbox_inches="tight")
    plt.close()
    print(f"[OK] 保存图：{outfile}")

def main():
    parser = argparse.ArgumentParser(
        description="自动扫描当前目录下 *_out_voq_cdf.txt 与 *_out_voq_per_dst_cdf.txt，并各画一张 CDF 图。"
    )
    parser.add_argument("--root", default=".", help="扫描起始目录（默认当前目录）")
    parser.add_argument("--figdir", default="./figures", help="输出图片目录（默认 ./figures）")
    args = parser.parse_args()

    voq_files, per_dst_files = find_cdf_files(args.root)

    print("=== 扫描结果 ===")
    print("[VOQ CDF 文件]:")
    for p in voq_files:
        print("  -", p)
    print("[VOQ per-dst CDF 文件]:")
    for p in per_dst_files:
        print("  -", p)

    # 读取并组织数据
    voq_series = {}
    for p in voq_files:
        xs, ys = read_cdf_file(p)
        if xs and ys:
            voq_series[label_from_path(p)] = (xs, ys)

    per_dst_series = {}
    for p in per_dst_files:
        xs, ys = read_cdf_file(p)
        if xs and ys:
            per_dst_series[label_from_path(p)] = (xs, ys)

    # 作图
    plot_many(
        voq_series,
        title="VOQ Memory Usage CDF (per-switch)",
        xlab="Memory Usage (Packets)",
        ylab="CDF",
        outfile=os.path.join(args.figdir, "voq_cdf_all.pdf"),
    )

    plot_many(
        per_dst_series,
        title="VOQ Memory Usage CDF (per-destination)",
        xlab="Memory Usage (Packets)",
        ylab="CDF",
        outfile=os.path.join(args.figdir, "voq_per_dst_cdf_all.pdf"),
    )

if __name__ == "__main__":
    main()

