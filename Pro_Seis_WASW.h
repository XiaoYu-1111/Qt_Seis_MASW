#pragma once

#include <QMainWindow>
#include <vector>
#include <memory>

#include "Pro_h/Plot1D.h"

class QTabWidget;
class QDockWidget;
class QCustomPlot;
class QTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QPushButton;

namespace Ui {
    class Pro_Seis_WASWClass;
}

class Pro_Seis_WASW : public QMainWindow
{
    Q_OBJECT

public:
    explicit Pro_Seis_WASW(QWidget* parent = nullptr);
    ~Pro_Seis_WASW() override;

private:
    void initUI();
    void initControlDock();
    void initMainTabs();
    void initLogDock();
    void createActionsAndToolBars(); // 新增：初始化菜单与工具栏

private slots:
    void onOpenSegy();         // 新增：打开并载入 SEGY
    void onCalculateClicked();

private:
    Ui::Pro_Seis_WASWClass* ui;

    // 工作区
    QTabWidget* mainTabWidget = nullptr;
    QWidget* seismicViewContainer = nullptr; // 用于盛放 SeismicView2D 的容器
    QCustomPlot* plotDispersion = nullptr;
    Data_show::Plot1D* plotCurve1D = nullptr;

    // 地震数据缓存（核心数据，供后续频散变换使用）
    std::vector<std::vector<float>> m_seismicData;

    // 参数面板控件
    QDockWidget* controlDock = nullptr;
    QDoubleSpinBox* spinDx = nullptr;
    QDoubleSpinBox* spinVmin = nullptr;
    QDoubleSpinBox* spinVmax = nullptr;
    QDoubleSpinBox* spinFreqMax = nullptr;
    QComboBox* comboMethod = nullptr;
    QPushButton* btnCalculate = nullptr;
    // 在 Pro_Seis_WASW.h 的 private 成员变量中增加：
    QDoubleSpinBox* spinDt = nullptr;       // 采样间隔 (ms)
    QDoubleSpinBox* spinOffset0 = nullptr;  // 最小炮检距 (m)
    QDoubleSpinBox* spinFmin = nullptr;     // 起始分析频率 (Hz)

    QComboBox* comboGridQuality = nullptr; // 网格质量选择框

    // 日志控制台
    QDockWidget* logDock = nullptr;
    QTextEdit* textLog = nullptr;

    // 在 Pro_Seis_WASW.h 的 private 成员变量中增加：

// --- 页面 2: 频散能量谱相关控件 ---
    QWidget* dispersionContainer = nullptr;   // 页面2整体容器
    QCPColorMap* dispColorMap = nullptr;       // 频散热力图层指针
    QCPColorScale* dispColorScale = nullptr;   // 色标条指针
    QComboBox* comboDispCmap = nullptr;        // 色标下拉选择框
    QCheckBox* chkDispInv = nullptr;           // 色标反转复选框
    QLabel* lblDispStatus = nullptr;           // 鼠标十字光标状态显示 (Freq, Vel, Energy)

    // --- 页面 2: 归一化与能量缓存 ---
    QComboBox* comboNormMode = nullptr; // 归一化模式下拉框 (按频率 / 全局)

    // 缓存计算结果与几何参数（用于不重算、仅瞬间切换显示）
    std::vector<std::vector<float>> m_rawDispersionEnergy;
    float m_dispFmin = 0, m_dispFmax = 0, m_dispVmin = 0, m_dispVmax = 0;
    int m_dispNf = 0, m_dispNv = 0;

    // 私有函数
    void renderDispersionMap(); // 根据当前归一化模式瞬间渲染显示

    // 私有辅助函数
    void updateDispersionColormap();           // 刷新频散图色标

    // 在 Pro_Seis_WASW.h 的 private 成员变量区域增加：

// --- 频散曲线拾取与联动控件 ---
    QCPGraph* pickGraphOnDispersion = nullptr; // 页面2热力图上的覆盖折线图层
    QCheckBox* chkPickMode = nullptr;          // 开启/关闭拾取模式复选框
    QPushButton* btnUndoPick = nullptr;        // 撤销上一点
    QPushButton* btnClearPick = nullptr;       // 清空所有拾取点
    QPushButton* btnExportCurve = nullptr;     // 导出频散曲线按钮

    // 存储拾取的频散点集合 (X: 频率 Hz, Y: 相速度 m/s)
    QVector<QPointF> m_pickedPoints;

    // 私有槽函数/辅助函数
    void onDispersionPlotClicked(QMouseEvent* event); // 点击吸附算法
    void updatePickVisuals();                         // 同步刷新 Tab2 与 Tab3 视图
    void exportPickedCurve();                          // 导出数据文件
};
