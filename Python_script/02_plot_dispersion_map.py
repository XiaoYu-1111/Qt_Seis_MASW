import os
import matplotlib
matplotlib.use('Agg')  # 后台无界面渲染，稳定防卡死
import matplotlib.pyplot as plt
import numpy as np
import segyio

# ==============================================================================
# 1. 明确设定输入输出绝对路径 (用户配置区)
# ==============================================================================
INPUT_DIR = r"D:\Code\visual_code\QT_project\Qt_Seis_WASW\Pro_Seis_WASW\pro_data"
SEGY_FILE_NAME = "DispersionMap_F5-80Hz_V400-1000_20260920_220411.sgy"
SEGY_PATH = os.path.join(INPUT_DIR, SEGY_FILE_NAME)

# 【核心修改】：指定 output_figures 文件夹，若不存在则自动新建
OUTPUT_DIR = os.path.join(INPUT_DIR, "output_figures")
os.makedirs(OUTPUT_DIR, exist_ok=True)

base_name = os.path.splitext(SEGY_FILE_NAME)[0]
OUTPUT_IMAGE_PATH = os.path.join(OUTPUT_DIR, f"{base_name}_HD.png")

# 物理坐标范围（直接对应文件名中的参数）
F_MIN, F_MAX = 5.0, 80.0       # 频率 (Hz)
V_MIN, V_MAX = 400.0, 1000.0   # 相速度 (m/s)

# ==============================================================================
# 2. 文件有效性检查
# ==============================================================================
if not os.path.exists(SEGY_PATH):
    print(f"❌ 找不到输入文件: {SEGY_PATH}")
    exit(1)

print(f"/// 开始处理目标 SEGY 文件: [{SEGY_FILE_NAME}]")
print(f"    输出目标文件夹: {OUTPUT_DIR}")

# ==============================================================================
# 3. 标准 segyio 矩阵直读
# ==============================================================================
with segyio.open(SEGY_PATH, 'r', ignore_geometry=True) as f:
    n_traces = f.tracecount
    n_samples = len(f.samples)
    dt_us = f.bin[segyio.BinField.Interval]

    print(f"✔ SEGY 规格读取成功: {n_traces} 频率道 × {n_samples} 速度采样点")

    # 转置后为 (采样点数 Y, 道数 X)
    data = f.trace.raw[:].T

# ==============================================================================
# 4. 高清科研成图 (300 DPI + 双三次平滑插值)
# ==============================================================================
plt.rcParams['font.family'] = ['Arial', 'Segoe UI', 'SimSun']
plt.rcParams['axes.linewidth'] = 1.2

fig, ax = plt.subplots(figsize=(10, 6.5))

im = ax.imshow(
    data,
    aspect='auto',
    cmap='jet',
    origin='lower',
    extent=[F_MIN, F_MAX, V_MIN, V_MAX],
    interpolation='bicubic',  # 双三次平滑插值，消除像素毛刺
    vmin=0.0,
    vmax=1.0
)

# 色标条设置
cbar = fig.colorbar(im, ax=ax, pad=0.02, fraction=0.046)
cbar.set_label('Normalized Energy', fontsize=12, fontweight='bold')
cbar.ax.tick_params(labelsize=10, width=1.2)

# 坐标轴与标签
ax.set_xlabel("Frequency (Hz)", fontsize=13, fontweight='bold')
ax.set_ylabel("Phase Velocity (m/s)", fontsize=13, fontweight='bold')
ax.set_title(f"Dispersion Energy Spectrum (MASW)\n{SEGY_FILE_NAME}", fontsize=13, fontweight='bold', pad=12)

ax.tick_params(axis='both', which='both', labelsize=11, width=1.2, length=5, direction='out')

# 保存到 output_figures 文件夹
plt.savefig(OUTPUT_IMAGE_PATH, dpi=300, bbox_inches='tight')
plt.close(fig)

print(f"\n🎉 高清图件已成功保存至 output_figures 目录下:\n    {OUTPUT_IMAGE_PATH}")

# 5. 自动在 Windows 资源管理器中高亮选中生成的文件
try:
    os.system(f'explorer /select,"{OUTPUT_IMAGE_PATH}"')
except Exception:
    pass