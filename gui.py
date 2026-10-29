#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
无人机地面基站覆盖与功率优化系统 - 图形界面与可视化演示
数据结构与算法分析课程设计
"""

import sys
import os
import json
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import numpy as np

# Matplotlib GUI backend setup
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.patches import Circle

# 支持中文与负号显示
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'sans-serif']
plt.rcParams['axes.unicode_minus'] = False


class UAVVisualizerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("无人机地面基站覆盖与功率优化系统 - 图形界面演示")
        self.root.geometry("1280x880")
        self.root.minsize(1050, 750)

        # 路径与当前数据
        self.exe_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uav_solver.exe")
        self.json_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results.json")
        self.data = None

        self._build_ui()
        self._run_solver(seed=42)

    def _build_ui(self):
        # 1. 顶部控制栏
        ctrl_frame = ttk.LabelFrame(self.root, text=" 仿真与求解参数设置 ", padding=(12, 8))
        ctrl_frame.pack(side=tk.TOP, fill=tk.X, padx=12, pady=6)

        ttk.Label(ctrl_frame, text="地面用户数 N (≥8):").pack(side=tk.LEFT, padx=(4, 2))
        self.entry_n = ttk.Spinbox(ctrl_frame, from_=8, to=100, width=5)
        self.entry_n.set("16")
        self.entry_n.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Label(ctrl_frame, text="飞行高度 G (m):").pack(side=tk.LEFT, padx=(4, 2))
        self.entry_g = ttk.Entry(ctrl_frame, width=6)
        self.entry_g.insert(0, "20.0")
        self.entry_g.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Label(ctrl_frame, text="区域边长 (m):").pack(side=tk.LEFT, padx=(4, 2))
        self.entry_area = ttk.Entry(ctrl_frame, width=6)
        self.entry_area.insert(0, "100.0")
        self.entry_area.pack(side=tk.LEFT, padx=(0, 15))

        ttk.Label(ctrl_frame, text="随机种子:").pack(side=tk.LEFT, padx=(4, 2))
        self.entry_seed = ttk.Entry(ctrl_frame, width=8)
        self.entry_seed.insert(0, "42")
        self.entry_seed.pack(side=tk.LEFT, padx=(0, 20))

        btn_run = tk.Button(ctrl_frame, text="▶ 执行求解 (调用 C++ 引擎)", bg="#2B579A", fg="white",
                            font=("Microsoft YaHei", 9, "bold"), padx=10, pady=2, command=self._on_run_click)
        btn_run.pack(side=tk.LEFT, padx=6)

        btn_random = tk.Button(ctrl_frame, text="🎲 随机新样本", bg="#008272", fg="white",
                               font=("Microsoft YaHei", 9), padx=8, pady=2, command=self._on_random_click)
        btn_random.pack(side=tk.LEFT, padx=6)

        btn_save = tk.Button(ctrl_frame, text="📷 导出高清图", bg="#5C2D91", fg="white",
                             font=("Microsoft YaHei", 9), padx=8, pady=2, command=self._on_save_img_click)
        btn_save.pack(side=tk.LEFT, padx=6)

        # 2. 中间主区域 (左/右分割或绘图区 + 数据栏)
        content_frame = ttk.Frame(self.root)
        content_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=12, pady=4)

        # Matplotlib 画布
        self.fig, (self.ax1, self.ax2) = plt.subplots(1, 2, figsize=(11, 5.6), dpi=100)
        self.fig.patch.set_facecolor('#F9FAFC')
        self.fig.subplots_adjust(left=0.06, right=0.96, top=0.92, bottom=0.08, wspace=0.18)

        self.canvas = FigureCanvasTkAgg(self.fig, master=content_frame)
        self.canvas.get_tk_widget().pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # 3. 底部信息面板：指标对比表 + 链表数据日志
        bottom_frame = ttk.Frame(self.root)
        bottom_frame.pack(side=tk.BOTTOM, fill=tk.X, padx=12, pady=(0, 8))

        # 对比卡片
        self.compare_frame = ttk.LabelFrame(bottom_frame, text=" 方案性能量化对比指标 ", padding=(8, 4))
        self.compare_frame.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 8))

        cols = ("指标项", "方案 A (K-Means 基线)", "方案 B (模拟退火 SA)", "优化改善幅度")
        self.tree_comp = ttk.Treeview(self.compare_frame, columns=cols, show='headings', height=5)
        for col, width in zip(cols, [160, 150, 150, 110]):
            self.tree_comp.heading(col, text=col)
            self.tree_comp.column(col, width=width, anchor=tk.CENTER)
        self.tree_comp.pack(fill=tk.BOTH, expand=True)

        # 链表节点详情
        self.log_frame = ttk.LabelFrame(bottom_frame, text=" 用户链表节点数据详情 (香农容量反解与信道系数) ", padding=(8, 4))
        self.log_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        log_cols = ("用户ID", "坐标 (x, y)", "容量需求 Rn", "所属基站", "空间距离 dn", "信道系数 hn", "最小发射功率 pn")
        self.tree_users = ttk.Treeview(self.log_frame, columns=log_cols, show='headings', height=5)
        widths = [60, 95, 95, 75, 85, 95, 110]
        for col, width in zip(log_cols, widths):
            self.tree_users.heading(col, text=col)
            self.tree_users.column(col, width=width, anchor=tk.CENTER)

        scrollbar = ttk.Scrollbar(self.log_frame, orient=tk.VERTICAL, command=self.tree_users.yview)
        self.tree_users.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree_users.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    def _run_solver(self, seed=None):
        if not os.path.exists(self.exe_path):
            messagebox.showerror("错误", f"找不到编译生成的核心执行程序：\n{self.exe_path}\n请先在终端编译 main.cpp！")
            return

        try:
            n = int(self.entry_n.get())
            g = float(self.entry_g.get())
            area = float(self.entry_area.get())
            if seed is None:
                seed_str = self.entry_seed.get().strip()
                seed = int(seed_str) if seed_str else int(np.random.randint(1, 10000))
                self.entry_seed.delete(0, tk.END)
                self.entry_seed.insert(0, str(seed))
        except ValueError:
            messagebox.showerror("输入错误", "请输入合法的数值参数！")
            return

        cmd = [self.exe_path, str(seed), str(n), str(g), str(area), self.json_path]
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, check=True)
        except Exception as e:
            messagebox.showerror("调用失败", f"运行求解引擎出错：\n{e}")
            return

        if not os.path.exists(self.json_path):
            messagebox.showerror("文件错误", f"未找到输出的 {self.json_path}")
            return

        with open(self.json_path, "r", encoding="utf-8") as f:
            self.data = json.load(f)

        self._plot_results()
        self._update_tables()

    def _plot_results(self):
        if not self.data:
            return

        users = self.data["users"]
        km = self.data["kmeans"]
        sa = self.data["sa"]
        area = self.data["area_size"]
        G = self.data["G"]

        # 清除画布
        self.ax1.clear()
        self.ax2.clear()

        # 调色盘
        uav_colors = ["#E63946", "#1D3557"]  # 珊瑚红 / 深海蓝
        uav_bg_fills = ["#FFE8E8", "#E3EEFF"]

        # ----------------- 子图 1：K-Means 基线 -----------------
        self._draw_scenario(self.ax1, users, km, "方案 A：K-Means 基准聚类划分", uav_colors, uav_bg_fills, area)

        # ----------------- 子图 2：模拟退火创新优化 -----------------
        # 标出在 SA 中归属发生改变的边缘用户
        changed_ids = [i for i, (a1, a2) in enumerate(zip(km["assign"], sa["assign"])) if a1 != a2]
        self._draw_scenario(self.ax2, users, sa, "方案 B：模拟退火 (SA) Minimax 创新优化", uav_colors, uav_bg_fills, area, changed_ids)

        self.canvas.draw()

    def _draw_scenario(self, ax, users, scheme, title, colors, fills, area, highlight_ids=None):
        ax.set_title(title, fontsize=11, fontweight="bold", pad=10)
        ax.set_xlim(-5, area + 5)
        ax.set_ylim(-5, area + 5)
        ax.set_aspect('equal', adjustable='box')
        ax.grid(True, linestyle="--", alpha=0.45)
        ax.set_xlabel("X 坐标 (m)", fontsize=9)
        ax.set_ylabel("Y 坐标 (m)", fontsize=9)

        drones = scheme["drones"]
        assign = scheme["assign"]

        # 绘制无人机覆盖圆与中心
        for d in drones:
            cid = d["id"]
            c = Circle((d["x"], d["y"]), d["max_r"], facecolor=fills[cid], edgecolor=colors[cid],
                       linestyle="--", linewidth=1.8, alpha=0.35, label=f"UAV {cid+1} 覆盖圆 (r={d['max_r']:.1f}m)")
            ax.add_patch(c)
            # 无人机位置 (方块图标)
            ax.scatter(d["x"], d["y"], c=colors[cid], marker="s", s=110, edgecolors="black", linewidth=1.5, zorder=5)
            ax.annotate(f"UAV {cid+1}\n({d['x']:.1f}, {d['y']:.1f})\nP={d['total_power']:.0f}W",
                        (d["x"], d["y"]), textcoords="offset points", xytext=(0, 10),
                        ha="center", fontsize=8, fontweight="bold", color=colors[cid],
                        bbox=dict(boxstyle="round,pad=0.2", fc="white", ec=colors[cid], alpha=0.85))

        # 绘制地面用户散点与连接虚线
        for i, u in enumerate(users):
            d_id = assign[i]
            d = drones[d_id]
            # 连线
            ax.plot([u["x"], d["x"]], [u["y"], d["y"]], color=colors[d_id], linestyle=":", linewidth=0.8, alpha=0.5)

            # 散点
            is_changed = (highlight_ids is not None and i in highlight_ids)
            marker = "*" if is_changed else "o"
            size = 90 if is_changed else 45
            edge_c = "orange" if is_changed else "black"
            line_w = 1.8 if is_changed else 0.8

            ax.scatter(u["x"], u["y"], c=colors[d_id], marker=marker, s=size, edgecolors=edge_c, linewidth=line_w, zorder=4)
            label_text = f"U{u['id']}" if not is_changed else f"U{u['id']}★"
            ax.annotate(label_text, (u["x"], u["y"]), textcoords="offset points", xytext=(4, 4), fontsize=7.5, color="#222222")

        # 信息汇总卡片
        bottleneck_r = scheme["max_horizontal_r"]
        max_3d = scheme["max_3d_dist"]
        total_p = scheme["total_power"]
        summary_text = (f"瓶颈覆盖半径 max(r): {bottleneck_r:.2f} m\n"
                        f"最远用户空间距离: {max_3d:.2f} m\n"
                        f"系统总发射功耗: {total_p:.1f} W")
        ax.text(0.02, 0.02, summary_text, transform=ax.transAxes, fontsize=8.5,
                verticalalignment='bottom', bbox=dict(boxstyle='round,pad=0.4', fc='#FFFFFF', ec='#888888', alpha=0.9))

    def _update_tables(self):
        # 1. 更新对比表
        for item in self.tree_comp.get_children():
            self.tree_comp.delete(item)

        km = self.data["kmeans"]
        sa = self.data["sa"]

        r_km = km["max_horizontal_r"]
        r_sa = sa["max_horizontal_r"]
        r_diff = ((r_sa - r_km) / r_km) * 100.0

        d_km = km["max_3d_dist"]
        d_sa = sa["max_3d_dist"]
        d_diff = ((d_sa - d_km) / d_km) * 100.0

        p_km = km["total_power"]
        p_sa = sa["total_power"]
        p_diff = ((p_sa - p_km) / p_km) * 100.0

        rows = [
            ("瓶颈水平半径 max(r)", f"{r_km:.2f} m", f"{r_sa:.2f} m", f"{r_diff:+.2f}%"),
            ("最远空间距离 max(dn)", f"{d_km:.2f} m", f"{d_sa:.2f} m", f"{d_diff:+.2f}%"),
            ("无人机 1 功率 P1", f"{km['drones'][0]['total_power']:.1f} W", f"{sa['drones'][0]['total_power']:.1f} W", "-"),
            ("无人机 2 功率 P2", f"{km['drones'][1]['total_power']:.1f} W", f"{sa['drones'][1]['total_power']:.1f} W", "-"),
            ("系统总功耗 P_total", f"{p_km:.1f} W", f"{p_sa:.1f} W", f"{p_diff:+.2f}%")
        ]
        for r in rows:
            self.tree_comp.insert("", tk.END, values=r)

        # 2. 更新用户链表节点详情
        for item in self.tree_users.get_children():
            self.tree_users.delete(item)

        users = self.data["users"]
        sa_drones = sa["drones"]
        sa_assign = sa["assign"]
        G = self.data["G"]

        for u in users:
            uid = u["id"]
            did = sa_assign[uid]
            dr = sa_drones[did]
            dn = np.sqrt((u["x"] - dr["x"])**2 + (u["y"] - dr["y"])**2 + G**2)
            hn = 1.0 / (dn**2)
            pn = (2.0**u["R"] - 1.0) * (dn**2)

            self.tree_users.insert("", tk.END, values=(
                f"U{uid}",
                f"({u['x']:.1f}, {u['y']:.1f})",
                f"{u['R']:.2f} bps/Hz",
                f"UAV {did+1}",
                f"{dn:.2f} m",
                f"{hn:.5f}",
                f"{pn:.1f} W"
            ))

    def _on_run_click(self):
        self._run_solver()

    def _on_random_click(self):
        new_seed = int(np.random.randint(1, 99999))
        self.entry_seed.delete(0, tk.END)
        self.entry_seed.insert(0, str(new_seed))
        self._run_solver(seed=new_seed)

    def _on_save_img_click(self):
        file_path = filedialog.asksaveasfilename(
            defaultextension=".png",
            filetypes=[("PNG 图像", "*.png"), ("PDF 矢量图", "*.pdf"), ("所有文件", "*.*")],
            initialfile="uav_coverage_comparison.png"
        )
        if file_path:
            self.fig.savefig(file_path, dpi=300, bbox_inches='tight')
            messagebox.showinfo("保存成功", f"对比演示图已成功保存至：\n{file_path}")


def export_standalone_image(output_path="comparison_plot.png", seed=42, n=16, g=20.0, area=100.0):
    """用于无头模式或快速脚本生成高清对比图像"""
    exe = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uav_solver.exe")
    json_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results.json")
    subprocess.run([exe, str(seed), str(n), str(g), str(area), json_path], check=True, capture_output=True)

    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 6), dpi=300)
    fig.subplots_adjust(left=0.06, right=0.96, top=0.92, bottom=0.08, wspace=0.18)

    users = data["users"]
    km = data["kmeans"]
    sa = data["sa"]
    uav_colors = ["#E63946", "#1D3557"]
    uav_bg_fills = ["#FFE8E8", "#E3EEFF"]

    def draw_ax(ax, scheme, title, highlight_ids=None):
        ax.set_title(title, fontsize=12, fontweight="bold", pad=10)
        ax.set_xlim(-5, area + 5)
        ax.set_ylim(-5, area + 5)
        ax.set_aspect('equal', adjustable='box')
        ax.grid(True, linestyle="--", alpha=0.45)
        ax.set_xlabel("X 坐标 (m)", fontsize=10)
        ax.set_ylabel("Y 坐标 (m)", fontsize=10)

        for d in scheme["drones"]:
            cid = d["id"]
            c = Circle((d["x"], d["y"]), d["max_r"], facecolor=uav_bg_fills[cid], edgecolor=uav_colors[cid],
                       linestyle="--", linewidth=1.8, alpha=0.35)
            ax.add_patch(c)
            ax.scatter(d["x"], d["y"], c=uav_colors[cid], marker="s", s=120, edgecolors="black", linewidth=1.5, zorder=5)
            ax.annotate(f"UAV {cid+1}\n({d['x']:.1f}, {d['y']:.1f})\nP={d['total_power']:.0f}W",
                        (d["x"], d["y"]), textcoords="offset points", xytext=(0, 10),
                        ha="center", fontsize=8.5, fontweight="bold", color=uav_colors[cid],
                        bbox=dict(boxstyle="round,pad=0.2", fc="white", ec=uav_colors[cid], alpha=0.85))

        for i, u in enumerate(users):
            d_id = scheme["assign"][i]
            d = scheme["drones"][d_id]
            ax.plot([u["x"], d["x"]], [u["y"], d["y"]], color=uav_colors[d_id], linestyle=":", linewidth=0.8, alpha=0.5)
            is_changed = (highlight_ids is not None and i in highlight_ids)
            marker = "*" if is_changed else "o"
            size = 100 if is_changed else 50
            edge_c = "darkorange" if is_changed else "black"
            line_w = 1.8 if is_changed else 0.8
            ax.scatter(u["x"], u["y"], c=uav_colors[d_id], marker=marker, s=size, edgecolors=edge_c, linewidth=line_w, zorder=4)
            label_text = f"U{u['id']}" if not is_changed else f"U{u['id']}★"
            ax.annotate(label_text, (u["x"], u["y"]), textcoords="offset points", xytext=(4, 4), fontsize=8, color="#222222")

        b_r = scheme["max_horizontal_r"]
        m_3d = scheme["max_3d_dist"]
        t_p = scheme["total_power"]
        summary = (f"瓶颈覆盖半径 max(r): {b_r:.2f} m\n"
                   f"最远用户空间距离: {m_3d:.2f} m\n"
                   f"系统总发射功耗: {t_p:.1f} W")
        ax.text(0.02, 0.02, summary, transform=ax.transAxes, fontsize=9,
                verticalalignment='bottom', bbox=dict(boxstyle='round,pad=0.4', fc='#FFFFFF', ec='#888888', alpha=0.9))

    draw_ax(ax1, km, "方案 A：K-Means 基准聚类划分")
    changed = [i for i, (a1, a2) in enumerate(zip(km["assign"], sa["assign"])) if a1 != a2]
    draw_ax(ax2, sa, "方案 B：模拟退火 (SA) Minimax 创新优化", changed)

    fig.savefig(output_path, dpi=300, bbox_inches='tight')
    plt.close(fig)
    print(f">> 高清对比图像已成功保存至: {output_path}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--save-img":
        out = sys.argv[2] if len(sys.argv) > 2 else "uav_coverage_comparison.png"
        export_standalone_image(out)
    else:
        root = tk.Tk()
        app = UAVVisualizerApp(root)
        root.mainloop()
