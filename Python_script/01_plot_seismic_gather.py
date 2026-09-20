# -*- coding: utf-8 -*-

import os
import segyio
import numpy as np
import matplotlib.pyplot as plt

# ==============================================================================
# 1. 全局绘图环境配置 (论文级规范)
# ==============================================================================
plt.rcParams['font.family'] = ['Times New Roman', 'SimSun']  # 中英文字体兼容
plt.rcParams['axes.unicode_minus'] = False                  # 正常显示负号
plt.rcParams['xtick.top'] = True
plt.rcParams['xtick.labeltop'] = True                        # 标尺默认显示在顶部
plt.style.use('bmh')

# ==============================================================================
# 2. 核心函数模块
# ==============================================================================
def read_segy_data(filepath, verbose=True):
    #"""
    #读取SEGY格式地震数据并获取采样信息
    
    #返回:
     #   data2D (np.ndarray): 形状为 (nSample, nTrace) 的地震数据矩阵
     #   meta (dict): 包含道数、采样点数、采样间隔(ms)、总时长(ms)的字典
    #"""
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"未找到目标文件: {filepath}")

    if verbose:
        print(f"/// 正在读取 SEGY 数据: [{os.path.basename(filepath)}]")

    with segyio.open(filepath, 'r', ignore_geometry=True) as f:
        f.mmap()
        n_trace = f.tracecount
        n_sample = f.bin[segyio.BinField.Samples]
        
        # segyio 获取的 Interval 单位通常为微秒 (us)
        raw_interval = f.bin[segyio.BinField.Interval]
        
        # 兼容处理：若头文件缺失/异常，可按用户指定参数(800ms, 采样率1000 -> dt=1ms)
        if raw_interval <= 0:
            dt_ms = 1.0
            print("  [警告] 头文件采样间隔异常，自动回退为默认 dt = 1.0 ms")
        else:
            dt_ms = raw_interval / 1000.0  # 转为毫秒 (ms)
            
        # 转换为 (nSample, nTrace)
        data2D = np.asarray([np.copy(x) for x in f.trace[:]]).T

    total_time_ms = (data2D.shape[0] - 1) * dt_ms
    
    meta = {
        'n_trace': n_trace,
        'n_sample': data2D.shape[0],
        'dt_ms': dt_ms,
        'total_time_ms': total_time_ms
    }
    
    if verbose:
        print(f"  -> 道数: {meta['n_trace']}, 采样点: {meta['n_sample']}, "
              f"采样间隔: {meta['dt_ms']:.2f} ms, 总记录时间: {meta['total_time_ms']:.1f} ms")
        
    return data2D, meta


def insert_zeros(trace, tt):
    """线性插值寻找零交叉点，用于变面积填充 (Wiggle Fill)"""
    zc_idx = np.where(np.diff(np.signbit(trace)))[0]
    if len(zc_idx) == 0:
        return trace, tt

    x1, x2 = tt[zc_idx], tt[zc_idx + 1]
    y1, y2 = trace[zc_idx], trace[zc_idx + 1]
    
    # 避免除以零
    denom = np.where(x2 - x1 == 0, 1e-12, x2 - x1)
    a = (y2 - y1) / denom
    a = np.where(a == 0, 1e-12, a)
    tt_zero = x1 - y1 / a

    tt_split = np.split(tt, zc_idx + 1)
    trace_split = np.split(trace, zc_idx + 1)
    
    tt_zi = tt_split[0]
    trace_zi = trace_split[0]

    for i in range(len(tt_zero)):
        tt_zi = np.hstack((tt_zi, np.array([tt_zero[i]]), tt_split[i + 1]))
        trace_zi = np.hstack((trace_zi, np.zeros(1), trace_split[i + 1]))

    return trace_zi, tt_zi


def plot_wiggle_trace(ax, data, tt, xx, sf=1.0, color='k', linewidth=0.2):
    """
    在指定坐标轴上绘制 Wiggle 振幅填充波形
    """
    ts = np.min(np.diff(xx)) if len(xx) > 1 else 1.0
    data_std = np.max(np.std(data, axis=0))
    if data_std == 0:
        data_std = 1.0
    
    scaled_data = data / data_std * ts * sf

    for i, offset in enumerate(xx):
        trace = scaled_data[:, i]
        trace_zi, tt_zi = insert_zeros(trace, tt)
        
        # 填充正半周
        ax.fill_betweenx(tt_zi, offset, trace_zi + offset,
                         where=trace_zi >= 0,
                         facecolor=color, linewidth=linewidth)
        # 绘制波形线
        ax.plot(trace_zi + offset, tt_zi, color=color, linewidth=linewidth)

    ax.set_xlim(xx[0] - ts, xx[-1] + ts)
    ax.set_ylim(tt[0], tt[-1])
    ax.invert_yaxis()


def format_seismic_axes(ax, n_traces, total_time_ms, 
                        x_ticks_num=5, y_ticks_num=11, 
                        x_label="道号", y_label=r"$\it{t}/\rm{ms}$"):
    """统一设置符合地震学成图规范的坐标轴和图框"""
    # Y轴 (时间) 刻度设置
    y_ticks = np.linspace(0, total_time_ms, y_ticks_num)
    ax.set_yticks(y_ticks)
    ax.set_yticklabels([f"{int(round(t))}" for t in y_ticks], fontsize=13)
    ax.set_ylabel(y_label, fontsize=15, fontstyle='italic')

    # X轴 (道号) 刻度设置 (固定置于顶部)
    x_ticks = np.linspace(1, n_traces, x_ticks_num)
    ax.set_xticks(x_ticks)
    ax.set_xticklabels([f"{int(round(x))}" for x in x_ticks], fontsize=13)
    
    ax.tick_params(axis='x', which='both', bottom=False, top=True, labelbottom=False, labeltop=True)
    ax.set_xlabel(x_label, fontsize=15)
    ax.xaxis.set_label_position('top')
    ax.xaxis.set_ticks_position('top')

    # 图框线与背景
    ax.grid(False)
    for spine in ax.spines.values():
        spine.set_color('black')
        spine.set_linewidth(1.0)
    ax.patch.set_facecolor('white')


# ==============================================================================
# 3. 顶层绘图接口
# ==============================================================================
def export_seismic_profile(data, meta, output_dir, file_prefix, 
                           plot_type='both', sf=1.0, linewidth=0.2, 
                           figsize=(4.5, 8), dpi=300):
    """
    根据输入矩阵和元数据导出图片
    
    参数:
        plot_type (str): 'wiggle', 'gray', 或 'both'
    """
    os.makedirs(output_dir, exist_ok=True)
    
    n_samples, n_traces = data.shape
    dt_ms = meta['dt_ms']
    tt = np.arange(n_samples) * dt_ms
    xx = np.arange(1, n_traces + 1)
    total_time_ms = tt[-1]

    # --- 1. Wiggle 剖面 ---
    if plot_type in ['wiggle', 'both']:
        fig, ax = plt.subplots(figsize=figsize)
        plot_wiggle_trace(ax, data, tt, xx, sf=sf, linewidth=linewidth)
        format_seismic_axes(ax, n_traces, total_time_ms)
        
        save_path = os.path.join(output_dir, f"{file_prefix}_wiggle.png")
        plt.savefig(save_path, bbox_inches='tight', dpi=dpi)
        plt.show()
        plt.close(fig)
        print(f"  [成功导出] Wiggle 剖面: {save_path}")

    # --- 2. 灰度变密度剖面 (Imshow) ---
    if plot_type in ['gray', 'both']:
        fig, ax = plt.subplots(figsize=figsize)
        
        # 裁剪阈值可自适应 99% 分位数以避免异常值压制对比度
        v_max = np.percentile(np.abs(data), 99)
        v_min = -v_max
        
        # extent=[left, right, bottom, top]
        ax.imshow(data, aspect='auto', cmap='gray', vmin=v_min, vmax=v_max,
                  extent=[xx[0] - 0.5, xx[-1] + 0.5, total_time_ms, 0])
        
        format_seismic_axes(ax, n_traces, total_time_ms)
        
        save_path = os.path.join(output_dir, f"{file_prefix}_gray.png")
        plt.savefig(save_path, bbox_inches='tight', dpi=dpi)
        plt.show()
        plt.close(fig)
        print(f"  [成功导出] 灰度密度剖面: {save_path}")


# ==============================================================================
# 4. 执行流程与路径配置 (用户自定义区域)
# ==============================================================================
if __name__ == '__main__':
    # ------------------ 路径设置 ------------------
    # 输入与输出目录
    INPUT_DIR = r"D:\Code\visual_code\QT_project\Qt_Seis_WASW\Pro_Seis_WASW\pro_data"
    OUTPUT_DIR = os.path.join(INPUT_DIR, "output_figures")
    
    # 示例 SEGY 单文件
    SEGY_FILE_NAME = "auto_export_gather_vz.sgy"
    SEGY_PATH = os.path.join(INPUT_DIR, SEGY_FILE_NAME)

    # ------------------ 单炮/单分量展示 ------------------
    if os.path.exists(SEGY_PATH):
        # 1. 读取数据
        data, meta = read_segy_data(SEGY_PATH)
        
        # 如果需要手动裁剪到 800ms (针对当前800ms的需求)：
        # 如果数据时长大于800ms，可以截取；若数据本身就是800ms则保持完整即可
        target_time_ms = 800.0
        target_samples = int(round(target_time_ms / meta['dt_ms']))
        data_crop = data[:target_samples, :]
        meta['n_sample'] = data_crop.shape[0]
        meta['total_time_ms'] = (data_crop.shape[0] - 1) * meta['dt_ms']

        # 2. 导出图件 (Wiggle 与 灰度图)
        export_seismic_profile(
            data=data_crop,
            meta=meta,
            output_dir=OUTPUT_DIR,
            file_prefix="Single_Component",
            plot_type='both',       # 'wiggle', 'gray', 或 'both'
            sf=1.0,                 # 振幅放大倍数
            linewidth=0.15,         # 线宽
            figsize=(4.5, 8),       # 单分量推荐宽高比
            dpi=300
        )

    # ------------------ 双分量水平拼接示例 (Vz + Vx) ------------------
    PATH_VZ = os.path.join(INPUT_DIR, "60m_300hz_d1.5m_circle_C_vz.sgy")
    PATH_VX = os.path.join(INPUT_DIR, "60m_300hz_d1.5m_circle_C_vx.sgy")

    if os.path.exists(PATH_VZ) and os.path.exists(PATH_VX):
        print("\n/// 正在处理 Vz + Vx 拼接数据...")
        data_vz, meta_vz = read_segy_data(PATH_VZ, verbose=False)
        data_vx, _ = read_segy_data(PATH_VX, verbose=False)
        
        # 裁剪并拼接 (例如只取前100道并拼接)
        sample_limit = int(round(800.0 / meta_vz['dt_ms']))  # 限制到 800ms
        data_stack = np.hstack([data_vz[:sample_limit, :100], 
                                data_vx[:sample_limit, :100]])
        
        stack_meta = meta_vz.copy()
        stack_meta['n_trace'] = data_stack.shape[1]
        stack_meta['n_sample'] = data_stack.shape[0]
        stack_meta['total_time_ms'] = (data_stack.shape[0] - 1) * stack_meta['dt_ms']

        # 导出双分量拼图
        export_seismic_profile(
            data=data_stack,
            meta=stack_meta,
            output_dir=OUTPUT_DIR,
            file_prefix="Stack_VZ_VX",
            plot_type='both',
            sf=1.0,
            linewidth=0.1,
            figsize=(8.0, 8.0),     # 拼接后横向较宽，调整为 (8, 8)
            dpi=300
        )