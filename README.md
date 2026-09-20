
---

# SeisTool-WASW 面波频散分析与处理系统

[![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Framework-Qt%205%20%2F%20Qt%206-green.svg)](https://www.qt.io/)
[![Eigen](https://img.shields.io/badge/Math-Eigen3%20%2B%20FFT-orange.svg)](https://eigen.tuxfamily.org/)
[![OpenMP](https://img.shields.io/badge/Parallel-OpenMP-red.svg)](https://www.openmp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20(MSVC)-lightgrey.svg)]()

**SeisTool-WASW** 是一款面向近地表地球物理工程勘察的专业面波频散分析系统（MASW，Multichannel Analysis of Surface Waves）。系统集成了高精度地震数据读取、多模式道集可视化、多核并行频散能量谱计算、交互式局部极大值吸附拾取以及出版级科研图表成图功能。

### 输入数据
![Dashboard Screenshot](Pro_image/main1.png)
### 频散能量图
![Dashboard Screenshot](Pro_image/main2.png)
### 频散曲线
![Dashboard Screenshot](Pro_image/main3.png)
---

## 目录
- [核心功能特性](#核心功能特性)
- [系统架构与工作流](#系统架构与工作流)
- [核心算法原理](#核心算法原理)
- [开发环境与依赖项](#开发环境与依赖项)
- [编译与运行配置](#编译与运行配置)
- [项目目录结构](#项目目录结构)
- [数据输入与导出规范](#数据输入与导出规范)

---

## 核心功能特性

### 1. 专业地震数据读取与呈现（Tab 1: Shot Gather）
* **全格式 SEGY 读取**：支持 IBM Float（代码 1）及 IEEE Float（代码 5）大端序地震数据的快速解码，自动解析道头、采样间隔（$dt$）与总道数。
* **双层混合成像**：支持**变密度热力图（ColorMap）**与**地震抖动波形（Wiggle）/ 正半周变面积填充（Variable Area）**的无缝叠合显示。
* **视口裁剪与 LOD 加速**：针对上千道的大型数据，内置屏幕动态视口裁剪（Frustum Culling）和基于采样分辨率的自动降采样绘制（Level of Detail），确保缩放拖拽流畅无卡顿。
* **丰富色标系统**：内置 Jet、Turbo、Viridis、Seismic、Grayscale 等 12 种经典地学色标，支持一键色彩反转（Inv）。

### 2. 高效移相法频散计算（Tab 2: Dispersion Map）
* **相移法能量谱变换**：基于经典 Park (1998) 移相法原理，将空间-时间域 $u(x, t)$ 数据变换为高分辨率的频率-相速度 $E(f, v)$ 能量谱。
* **多核并行加速**：基于 `Eigen::FFT` 与 `OpenMP` 实现频域相移多线程扫描，计算耗时由传统解释型脚本的数秒缩减至毫秒级。
* **动态质量预设**：
  * `快速预览 (100 × 100)`：极速计算，用于野外实时调参。
  * `标准质量 (180 × 250)`：兼顾平滑度与耗时，常规处理推荐。
  * `高精度 (300 × 500)`：高细粒度网格，适合论文配图与精细反演。
* **双模式归一化（1毫秒免重算切换）**：
  * **按频率归一化（每列归一）**：自动消除源子波能量不均匀影响，使全频段能量峰值达到 $1.0$（红色），基阶与高阶条带清晰连续。
  * **全局归一化**：呈现真实的主频能量集中与空间衰减规律。

### 3. 智能曲线拾取与联动（Tab 2 $\leftrightarrow$ Tab 3）
* **局部极大值自动吸附（Snap-to-Peak）**：鼠标在能量谱红色脊线上点击时，算法自动在垂直方向（$\pm 15$ 个速度网格）搜索局部最大值并精准就位，彻底规避手抖误差。
* **实时双向图层联动**：
  * 页面二（热力图）上实时覆盖渲染高对比度的黄线与红白采样圆点；
  * 页面三（`Plot1D`）自动对拾取点按频率升序排列，生成期刊出版级标准的折线散点图。
* **撤销与清空机制**：支持误点单步撤销（Undo）、点位重复覆盖修正及一键重置。

### 4. 高级时频信号分析工具箱
内置了丰富的单道与多道信号处理算法：
* 连续小波变换（Morlet CWT，具备内存保护机制）
* S变换（Stockwell Transform）与短时傅里叶变换（STFT）
* 变分模态分解（VMD）
* 希尔伯特包络分析（Hilbert Envelope）
* 韦尔奇功率谱密度估计（Welch PSD）

---

## 系统架构与工作流

```
[原始地震数据输入 (*.sgy, *.segy)]
                │
                ▼
┌──────────────────────────────────────────────┐
│  Tab 1: 原始道集与剖面分析 (Shot Gather)      │
│  - Wiggle + 变面积填充 / 伪彩热力图 / 对比度增益│
└───────────────────────┬──────────────────────┘
                        │ 设置: dt, dx, x0, Fmin-Fmax, Vmin-Vmax
                        ▼
┌──────────────────────────────────────────────┐
│  Tab 2: 移相法相速度能量谱 (Dispersion Map)   │
│  - OpenMP 多核并行计算 / 色标切换 / 归一化切换│
│  - 鼠标悬停实时追踪 Freq, Vel, Energy 数值   │
│  - [开启拾取] -> 鼠标点选触发 Snap-to-Peak 吸附│
└───────────────────────┬──────────────────────┘
                        │ 实时数据流同步
                        ▼
┌──────────────────────────────────────────────┐
│  Tab 3: 频散曲线展示与导出 (Extracted Curve)  │
│  - Plot1D 期刊级展示 (封闭边框 Box Style)     │
│  - 导出 (Frequency, PhaseVelocity, Lambda)   │
└───────────────────────┬──────────────────────┘
                        │
                        ▼
            [用于后续 S波速度反演模块]
```

---

## 核心算法原理

### 移相法相速度叠加公式 (Park et al., 1998)
1. **时间向傅里叶变换与振幅归一化**：
   $$P(x_j, \omega) = \frac{U(x_j, \omega)}{|U(x_j, \omega)|}$$
2. **空间相位补偿叠加**：
   $$E(v, f) = \frac{1}{N} \left| \sum_{j=1}^{N} P(x_j, 2\pi f) \cdot \exp\left(i \frac{2\pi f \cdot x_j}{v}\right) \right|$$
   *其中 $x_j = x_0 + (j-1)\Delta x$ 为各检波器的实际物理偏移距。*

---

## 开发环境与依赖项

* **操作系统**：Windows 10 / 11 (x64)
* **GUI 框架**：Qt 5.12+ 或 Qt 6.x（Widgets 模块）
* **编译器**：Microsoft Visual Studio 2019 / 2022 (MSVC v142/v143)
* **核心依赖库**：
  * **Eigen 3.3+**：包含 `<unsupported/Eigen/FFT>` 模块（纯头文件库，负责复数与 FFT 运算）
  * **QCustomPlot 2.1+**：负责底层绘图渲染
  * **OpenMP**：多核并行计算加速

---

## 编译与运行配置

### Visual Studio 项目配置注意事项

1. **启用扩展对象格式 `/bigobj`**（解决 Eigen 复杂模板引起的 C1128 错误）：
   * 项目属性 $\to$ **C/C++** $\to$ **命令行** $\to$ 其他选项中添加：
     ```text
     /bigobj
     ```
2. **启用 UTF-8 源代码编码 `/utf-8`**（解决 MSVC 中文字符断句与 C2143 报错）：
   * 项目属性 $\to$ **C/C++** $\to$ **命令行** $\to$ 其他选项中添加：
     ```text
     /utf-8
     ```
3. **启用 OpenMP 多线程加速**：
   * 项目属性 $\to$ **C/C++** $\to$ **语言** $\to$ **OpenMP 支持** $\to$ 设为 **是 (/openmp)**。

---

## 项目目录结构

```text
Qt_Seis_WASW/
├── Pro_h/                        # 核心头文件
│   ├── Plot1D.h                  # 1D 专业科研图表组件 (QCustomPlot 派生类)
│   ├── QCPSeismicWiggle.h        # 地震波形起伏线与变面积填充 Plottable
│   ├── SeismicIO.h               # SEGY 数据底层解析与导出接口
│   ├── SeismicView2D.h            # 2D 剖面多功能查看器接口
│   └── SignalProcessingUtils.h   # 频散移相法、小波变换、STFT、VMD 算法库
├── Pro_cpp/                      # 算法与组件实现
│   ├── Plot1D.cpp
│   ├── QCPSeismicWiggle.cpp
│   ├── SeismicIO.cpp
│   ├── SeismicView2D.cpp
│   └── SignalProcessingUtils.cpp
├── style.h                       # 全局 Slate 深色科技风格 QSS 样式表
├── Pro_Seis_WASW.h               # 主窗口逻辑控制器声明
├── Pro_Seis_WASW.cpp             # 业务流程、UI交互与拾取逻辑实现
├── main.cpp                      # 应用程序入口
└── README.md                     # 项目使用说明文档
```

---

## 数据输入与导出规范

### 1. 输入数据
* 支持标准地震勘探 **SEG-Y / SGY** 格式（二维主动源面波道集，Common Shot Gather）。
* 兼容单炮记录、微动台阵线性排列记录。

### 2. 导出数据格式
点击 **【导出曲线...】** 导出的文本文件（`.txt` 或 `.csv`）采用标准三列地学格式，可直接供 CPS330 (`surf96`)、Geopsy 或自编反演模块调用：

```text
# Frequency(Hz)    PhaseVelocity(m/s)    Wavelength(m)
5.000              324.50                64.90
7.500              298.20                39.76
10.000             276.40                27.64
12.500             255.80                20.46
...                ...                   ...
```

---

## 许可证与致谢

本项目基于科研与工程应用开发。底层绘图组件遵循 **GPLv3 / QCustomPlot 商业许可**；数据数学变换基于 **Eigen** 开源矩阵库构建。