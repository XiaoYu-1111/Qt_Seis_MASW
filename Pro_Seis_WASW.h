#pragma once

#include <QMainWindow>
#include <vector>
#include <memory>
#include <QVector>
#include <QPointF>

#include "Pro_h/Plot1D.h"

// =========================================================
// 前置声明 (Forward Declarations)
// =========================================================
class QTabWidget;
class QDockWidget;
class QCustomPlot;
class QTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QMouseEvent;

class QCPColorMap;
class QCPColorScale;
class QCPGraph;

namespace Ui {
    class Pro_Seis_WASWClass;
}

// =========================================================
// 主窗口类定义
// =========================================================
class Pro_Seis_WASW : public QMainWindow
{
    Q_OBJECT

public:
    explicit Pro_Seis_WASW(QWidget* parent = nullptr);
    ~Pro_Seis_WASW() override;

private:
    // --- 界面初始化方法 ---
    void initUI();
    void initControlDock();
    void initMainTabs();
    void initLogDock();
    void createActionsAndToolBars();

    // --- 频散图渲染与联动辅助方法 ---
    void renderDispersionMap();                       // 根据当前归一化与参数渲染频散图
    void updateDispersionColormap();                  // 切换色标 / 反转色标
    void updatePickVisuals();                         // 同步刷新 Tab 2 覆盖线与 Tab 3 频散曲线
    void exportPickedCurve();                         // 导出拾取的频散曲线文本 (txt/csv)
    void exportDispersionToSegy();                    // 导出二维频散能量谱为 SEGY 格式

private slots:
    // --- 核心业务槽函数 ---
    void onOpenSegy();                                // 打开并载入 SEGY 地震道集
    void onCalculateClicked();                        // 开始计算频散谱
    void onSyntheticClicked();                        // 一键合成理论面波记录
    void onDispersionPlotClicked(QMouseEvent* event); // 频散图鼠标点击与自动吸附拾取

private:
    Ui::Pro_Seis_WASWClass* ui;

    // =====================================================
    // 1. 中央工作区多标签页 (Central Tab Widgets)
    // =====================================================
    QTabWidget* mainTabWidget = nullptr;
    QWidget* seismicViewContainer = nullptr;          // Tab 1: 盛放 SeismicView2D 的容器
    QWidget* dispersionContainer = nullptr;           // Tab 2: 盛放频散热力图与底栏的容器
    QCustomPlot* plotDispersion = nullptr;            // Tab 2: 频散能量谱图表对象
    Data_show::Plot1D* plotCurve1D = nullptr;         // Tab 3: 提取的频散曲线对比 (Plot1D)

    // =====================================================
    // 2. 核心数据缓存 (Data Buffers)
    // =====================================================
    std::vector<std::vector<float>> m_seismicData;    // 原始地震道集数据 [Traces][Samples]
    std::vector<std::vector<float>> m_rawDispersionEnergy; // 计算出的原始频散能量矩阵 [Nv][Nf]
    QVector<QPointF> m_pickedPoints;                  // 已拾取的频散点集合 (x: 频率, y: 相速度)

    // 当前计算缓存的网格几何参数 (用于不重算即刻刷新显示)
    float m_dispFmin = 0.0f;
    float m_dispFmax = 0.0f;
    float m_dispVmin = 0.0f;
    float m_dispVmax = 0.0f;
    int   m_dispNf = 0;
    int   m_dispNv = 0;

    // =====================================================
    // 3. 左侧控制面板 (Control Dock) 控件
    // =====================================================
    QDockWidget* controlDock = nullptr;

    // --- Tab 1: 频散分析参数 (Dispersion Analysis) ---
    QDoubleSpinBox* spinDt = nullptr;                 // 采样间隔 (ms)
    QDoubleSpinBox* spinDx = nullptr;                 // 道间距 (m)
    QDoubleSpinBox* spinOffset0 = nullptr;            // 最小炮检距 (m)
    QComboBox* comboMethod = nullptr;            // 计算方法 (移相法 / F-K法)
    QComboBox* comboGridQuality = nullptr;       // 计算网格精细度预设
    QDoubleSpinBox* spinFmin = nullptr;               // 频率下限 (Hz)
    QDoubleSpinBox* spinFreqMax = nullptr;            // 频率上限 (Hz)
    QDoubleSpinBox* spinVmin = nullptr;               // 相速度下限 (m/s)
    QDoubleSpinBox* spinVmax = nullptr;               // 相速度上限 (m/s)
    QPushButton* btnCalculate = nullptr;           // 开始计算按钮

    // --- Tab 2: 理论正演模型参数 (1D Model Synthesis) ---
    // 找到 Pro_Seis_WASW.h 中的理论正演模型参数区域，修改/替换为：
    QDoubleSpinBox* spinLayerH1 = nullptr;   // 第 1 层厚度 H1 (m)
    QDoubleSpinBox* spinLayerVs1 = nullptr;  // 第 1 层横波速度 Vs1 (m/s)
    QDoubleSpinBox* spinLayerH2 = nullptr;   // 【新增】第 2 层厚度 H2 (m)
    QDoubleSpinBox* spinLayerVs2 = nullptr;  // 【新增】第 2 层横波速度 Vs2 (m/s)
    QDoubleSpinBox* spinLayerVs3 = nullptr;  // 第 3 层(基底半空间)横波速度 Vs3 (m/s)
    QDoubleSpinBox* spinWaveletFm = nullptr; // 雷克子波主频 Fm (Hz)

    // =====================================================
    // 4. Tab 2 (频散能量谱) 底栏与交互控制
    // =====================================================
    QCPColorMap* dispColorMap = nullptr;            // 频散热力图层指针
    QCPColorScale* dispColorScale = nullptr;          // 右侧色标条指针
    QCPGraph* pickGraphOnDispersion = nullptr;   // 热力图上方覆盖的拾取曲线图层
    QLabel* lblDispStatus = nullptr;           // 光标悬停坐标显示 (Freq, Vel, Energy)
    QComboBox* comboNormMode = nullptr;           // 归一化模式下拉框 (每列归一 / 全局归一)
    QComboBox* comboDispCmap = nullptr;           // 色标下拉选择框 (Jet, Turbo, Viridis...)
    QCheckBox* chkDispInv = nullptr;              // 色标反转复选框
    QCheckBox* chkPickMode = nullptr;             // 拾取模式切换开关
    QPushButton* btnUndoPick = nullptr;             // 撤销上一个拾取点
    QPushButton* btnClearPick = nullptr;            // 清空当前拾取曲线
    QPushButton* btnExportCurve = nullptr;          // 导出频散曲线数据按钮
    bool           m_interpolateColorMap = false;     // 平滑插值开关 (默认 false 呈现清晰色块)

    // =====================================================
    // 5. 底部运行日志面板 (Console Log Dock)
    // =====================================================
    QDockWidget* logDock = nullptr;
    QTextEdit* textLog = nullptr;
};