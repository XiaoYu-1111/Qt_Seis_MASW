import os
import numpy as np
import matplotlib.pyplot as plt
from disba import PhaseDispersion
from scipy.optimize import minimize

# ==============================================================================
# 1. 读取从 Qt 端导出的频散曲线文本 (.txt / .csv)
# ==============================================================================
DISP_FILE_PATH = r"D:\Code\visual_code\QT_project\Qt_Seis_MASW\Pro_Seis_MASW\pro_data\output_curve\DispersionCurve_20260922_192359.txt"

if not os.path.exists(DISP_FILE_PATH):
    print(f"❌ 未找到指定文件: {DISP_FILE_PATH}")
    exit(1)

# 读取数据: # Frequency(Hz) \t PhaseVelocity(m/s) \t Wavelength(m)
data = np.loadtxt(DISP_FILE_PATH, comments="#")
raw_f = data[:, 0]  # 原始频率 (Hz)
raw_v = data[:, 1]  # 原始观测相速度 (m/s)

# 按周期升序排列 (disba 硬性要求)
raw_periods = 1.0 / raw_f
sort_idx = np.argsort(raw_periods)

periods = raw_periods[sort_idx]
f_obs = raw_f[sort_idx]
v_obs = raw_v[sort_idx]

print("=" * 60)
print(f"✔ 成功载入观测频散曲线:")
print(f"   • 频点总数: {len(f_obs)} 个")
print(f"   • 频率范围: [{f_obs.min():.2f}, {f_obs.max():.2f}] Hz")
print(f"   • 相速度范围: [{v_obs.min():.1f}, {v_obs.max():.1f}] m/s")
print("=" * 60)

# ==============================================================================
# 2. 地层分层模型参数化 (定义反演网格)
# ==============================================================================
# 划分 6 层 (前 5 层厚度渐进，最后一层为无限深基底)
thicknesses = np.array([2.0, 3.0, 5.0, 8.0, 12.0, 0.0])  # 单位: 米
n_layers = len(thicknesses)

# 初始横波速度猜测 (根据观测速度平滑递增)
vs_initial = np.linspace(v_obs.min() * 1.05, v_obs.max() * 1.15, n_layers)

# 【核心修复】：标准 SciPy 边界格式 [(min, max), (min, max), ...]
bounds = [(100.0, 1500.0) for _ in range(n_layers)]

# ==============================================================================
# 3. 正演引擎封装 (调用 disba / surf96，含鲁棒容错)
# ==============================================================================
def forward_dispersion(vs_model):
    thk = thicknesses.copy()
    thk[-1] = 0.0  # 半空间厚度设为 0
    
    thk_km = thk / 1000.0
    vs_kms = np.clip(vs_model, 80.0, 3000.0) / 1000.0  # 防极值溢出
    vp_kms = vs_kms * 2.0  # 泊松比假设 Vp = 2 * Vs
    rho = np.ones(n_layers) * 2.0
    
    try:
        pd = PhaseDispersion(thk_km, vp_kms, vs_kms, rho)
        c = pd(periods, mode=0, wave="rayleigh")
        res_v = c.velocity * 1000.0
        if np.isnan(res_v).any():
            return np.full_like(periods, 1e5)
        return res_v
    except Exception:
        # 正演数值异常时返回极大惩罚，引导优化器回退
        return np.full_like(periods, 1e5)

# ==============================================================================
# 4. 构建带平滑正则化的目标函数 (Tikhonov Regularization)
# ==============================================================================
# 构造一阶平滑差分矩阵 L
L = np.zeros((n_layers - 1, n_layers))
for i in range(n_layers - 1):
    L[i, i] = -1.0
    L[i, i + 1] = 1.0

lambda_reg = 0.02  # 平滑度阻尼因子

def objective_func(m):
    pred_v = forward_dispersion(m)
    # 1. 数据拟合残差 (方差和的一半)
    data_misfit = 0.5 * np.sum((v_obs - pred_v) ** 2)
    # 2. 地层粗糙度惩罚项 (防止速度剧烈抖动)
    roughness = 0.5 * lambda_reg * np.sum((L @ m) ** 2)
    return data_misfit + roughness

# ==============================================================================
# 5. 执行高效有界非线性反演优化 (L-BFGS-B)
# ==============================================================================
print("🚀 正在启动频散反演优化计算 (L-BFGS-B)...")

opt_res = minimize(
    fun=objective_func,
    x0=vs_initial,
    method="L-BFGS-B",
    bounds=bounds,
    options={"maxiter": 120, "disp": False}
)

vs_inverted = opt_res.x
v_pred_final = forward_dispersion(vs_inverted)

rmse = np.sqrt(np.mean((v_obs - v_pred_final) ** 2))
print("=" * 60)
print(f"🎉 反演成功收敛！(迭代次数: {opt_res.nit} 次)")
print(f"• 最终频散拟合均方根误差 (RMSE): {rmse:.2f} m/s")
for i in range(n_layers):
    layer_name = f"第 {i+1} 层 (厚度 {thicknesses[i]:.1f} m)" if i < n_layers - 1 else "基底半空间 (无限深)"
    print(f"  └─ {layer_name:<20}: Vs = {vs_inverted[i]:.1f} m/s")
print("=" * 60)

# ==============================================================================
# 6. 反演成果可视化 (频散曲线拟合度 + 1D Vs 深度台阶剖面)
# ==============================================================================
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.5))

# 图 A: 频散曲线拟合对比 (按频率由小到大展示)
freq_sort = np.argsort(f_obs)
ax1.plot(f_obs[freq_sort], v_obs[freq_sort], 'ro', label='Observed Curve (Qt Picked)', markersize=5, alpha=0.7)
ax1.plot(f_obs[freq_sort], forward_dispersion(vs_initial)[freq_sort], 'b--', label='Initial Model Curve', lw=1.5)
ax1.plot(f_obs[freq_sort], v_pred_final[freq_sort], 'k-', label=f'Inverted Fit (RMSE={rmse:.2f} m/s)', lw=2.2)
ax1.set_xlabel("Frequency (Hz)", fontsize=11, fontweight='bold')
ax1.set_ylabel("Phase Velocity (m/s)", fontsize=11, fontweight='bold')
ax1.set_title("Rayleigh Wave Dispersion Curve Fitting", fontsize=12, fontweight='bold')
ax1.grid(True, linestyle=":", alpha=0.6)
ax1.legend(loc='upper right')

# 图 B: 地下深度 vs 横波速度阶梯剖面 (1D Vs Profile)
depths_plot = [0.0]
vs_plot = [vs_inverted[0]]

cum_depth = 0.0
for i in range(n_layers - 1):
    cum_depth += thicknesses[i]
    depths_plot.extend([cum_depth, cum_depth])
    vs_plot.extend([vs_inverted[i], vs_inverted[i+1]])

depths_plot.append(cum_depth + 10.0) # 半空间向下画 10 米
vs_plot.append(vs_inverted[-1])

ax2.step(vs_plot, depths_plot, 'b-', lw=2.2, label='Inverted Vs Profile', where='post')
ax2.set_xlabel("Shear-Wave Velocity Vs (m/s)", fontsize=11, fontweight='bold')
ax2.set_ylabel("Depth (m)", fontsize=11, fontweight='bold')
ax2.set_title("Inverted 1D S-Wave Velocity Profile", fontsize=12, fontweight='bold')
ax2.invert_yaxis() # 深度向下递增
ax2.grid(True, linestyle=":", alpha=0.6)
ax2.legend(loc='lower left')

plt.tight_layout()
plt.show()

# ==============================================================================
# 7. 导出反演地质模型 (供 C++ Qt 端读取)
# ==============================================================================
OUT_PROFILE_PATH = r"D:\Code\visual_code\QT_project\Qt_Seis_MASW\Pro_Seis_MASW\pro_data\output_curve\Inverted_Vs_Model.txt"
with open(OUT_PROFILE_PATH, "w", encoding="utf-8") as f:
    f.write("# Layer\tThickness(m)\tDepth_Top(m)\tVs(m/s)\tVp(m/s)\tDensity(kg/m3)\n")
    top_z = 0.0
    for i in range(n_layers):
        thk = thicknesses[i]
        f.write(f"{i+1}\t{thk:.2f}\t{top_z:.2f}\t{vs_inverted[i]:.2f}\t{vs_inverted[i]*2:.2f}\t2000.0\n")
        top_z += thk

print(f"✔ 反演地层模型已保存至: {OUT_PROFILE_PATH}")