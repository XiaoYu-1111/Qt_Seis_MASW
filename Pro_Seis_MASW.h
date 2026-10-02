#pragma once

#include <QMainWindow>
#include <vector>
#include <memory>
#include <QVector>
#include <QPointF>
#include <QProgressBar>
#include <QLineEdit>
#include <QTextStream>
#include <QHeaderView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDockWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QDateTime>
#include <QFileDialog>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>

// 引入模块
#include "Pro_h/SeismicIO.h"
#include "Pro_h/Plot1D.h"
#include "Pro_h/SeismicView2D.h"
#include "Pro_h/SignalProcessingUtils.h"
#include "Pro_h/RayleighForwardSolver.h"

#include "Pro_h/RayleighInversionSolver.h"

#include <QThread>
#include <fstream>
#include <random>

// 引入 ONNX Runtime C++ API
#include <onnxruntime_cxx_api.h>

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
class QTableWidget;

class QCPColorMap;
class QCPColorScale;
class QCPGraph;



namespace Ui {
    class Pro_Seis_MASWClass;
}

// =========================================================
// 主窗口类定义
// =========================================================
class Pro_Seis_MASW : public QMainWindow
{
    Q_OBJECT

public:
    explicit Pro_Seis_MASW(QWidget* parent = nullptr);
    ~Pro_Seis_MASW() override;

private:
    // --- 界面初始化方法 ---
    void initUI();
    void initControlDock();
    void initMainTabs();
    void initLogDock();
    void createActionsAndToolBars();

    void loadSegyFile(const QString& filePath); // 核心：统一的 SEGY 文件解析与载入

    // --- 频散图渲染与联动辅助方法 ---
    void renderDispersionMap();                       // 根据当前归一化与参数渲染频散图
    void updateDispersionColormap();                  // 切换色标 / 反转色标
    void updatePickVisuals();                         // 同步刷新 Tab 2 覆盖线与 Tab 3 频散曲线
    void exportPickedCurve();                         // 导出拾取的频散曲线文本 (txt/csv)
    void exportDispersionToSegy();                    // 导出二维频散能量谱为 SEGY 格式
    void displayInversionModel(const QString& filePath); // 解析并绘制模型

    void updateDataBadges(const QString& fileName, int traces, int samples, float dtMs);

    void initStatusBar();                 // 初始化仪器级状态栏

    // 辅助方法：多曲线自适应缩放重绘
    void updateCurveComparisonView();
    void update2DVsSection();

private slots:
    // --- 核心业务槽函数 ---
    void onOpenSegy();                                // 打开并载入 SEGY 地震道集
    void onCalculateClicked();                        // 开始计算频散谱
    void onSyntheticClicked();                        // 一键合成理论面波记录
    void onDispersionPlotClicked(QMouseEvent* event); // 频散图鼠标点击与自动吸附拾取

    void onStartDatasetGeneration(); // 开始批量生成数据集
    void onAiPickClicked(); // AI 一键自动拾取槽函数

    void onLoadInversionModel(); // 载入反演模型文件槽函数
    void onRunInversionClicked(); // 【原生 C++ 1D 速度反演】槽函数

    void onResetAll();        // 一键重置全部视图
    void onShowHelp();        // 显示系统帮助与说明

    

private:
    Ui::Pro_Seis_MASWClass* ui;

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
    // 找到 Pro_Seis_MASW.h 中的理论正演模型参数区域，修改/替换为：
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
// Tab 3: 频散曲线对比多图层与独立显隐控制
// =====================================================
    QWidget* curveCompareContainer = nullptr; // Tab 3 总容器
    QCPGraph* graphTheoretical = nullptr; // 图层1: 理论基阶曲线 (红实线)
    QCPGraph* graphPicked = nullptr; // 图层2: 实测拾取点 (蓝线红点)
    QCPGraph* graphInverted = nullptr; // 图层3: 反演拟合曲线 (黑虚线)

    QCheckBox* chkShowTheoretical = nullptr; // 控制理论线显隐
    QCheckBox* chkShowPicked = nullptr; // 控制拾取线显隐
    QCheckBox* chkShowInverted = nullptr; // 控制反演线显隐
    QLabel* lblCurveMisfit = nullptr; // 实时显示拟合残差 RMSE

    // =====================================================
    // 5. 底部运行日志面板 (Console Log Dock)
    // =====================================================
    QDockWidget* logDock = nullptr;
    QTextEdit* textLog = nullptr;

    // =====================================================
    // Tab 3: 数据集批量生成控件 (Dataset Generation)
    // =====================================================
    QSpinBox* spinSampleCount = nullptr;  // 生成样本数量 (例如 1000)
    QLineEdit* editOutputDir = nullptr;  // 输出目录
    QPushButton* btnBrowseDir = nullptr;  // 浏览目录按钮
    QProgressBar* progressGen = nullptr;  // 生成进度条
    QPushButton* btnStartGen = nullptr;  // 启动生成按钮

    QPushButton* btnAiPick = nullptr; // AI 一键拾取按钮

    // =====================================================
    // 6. Tab 4: 1D 速度剖面与地层反演展示控件
    // =====================================================
    QWidget* inversionContainer = nullptr; // Tab 4 总容器
    Data_show::Plot1D* plotVsProfile = nullptr; // 1D 阶梯剖面图表
    QTableWidget* tableVsModel = nullptr;

    struct VsModelLayer {
        double topDepth = 0.0;
        double thickness = 0.0;
        double vs = 0.0;
    };
    QVector<VsModelLayer> m_vsModelLayers;

    QWidget* section2DContainer = nullptr;
    QCustomPlot* plotVsSection = nullptr;
    QCPColorMap* vsSectionColorMap = nullptr;
    QCPColorScale* vsSectionColorScale = nullptr;
    QDoubleSpinBox* spinSectionLength = nullptr;
    QDoubleSpinBox* spinSectionVariation = nullptr;
    QSpinBox* spinSectionNx = nullptr;
    QSpinBox* spinSectionNz = nullptr;
    QComboBox* comboSectionPalette = nullptr;
    QPushButton* btnGenerateSection = nullptr;
    QLabel* lblSectionStatus = nullptr; // 地层参数表格
    QPushButton* btnLoadVsModel = nullptr; // 载入模型文件按钮
    QLabel* lblVsSummary = nullptr; // 底部模型状态摘要

    QPushButton* btnRunInversion = nullptr; // 开始 1D 速度反演按钮

    QComboBox* comboInvLayers = nullptr; // 反演分层方案选择框 (两层/三层/六层...)

    // =====================================================
    // 7. 顶部工具栏数据看板胶囊标签 (Data Badges)
    // =====================================================
    QLabel* lblBadgeFile = nullptr; // 当前文件名
    QLabel* lblBadgeTraces = nullptr; // 总道数
    QLabel* lblBadgeDt = nullptr; // 采样率 dt
    QLabel* lblBadgeTime = nullptr; // 总记录时长

    // =====================================================
// 8. 底部状态栏地学仪器级状态芯片 (Status Chips)
// =====================================================
    QLabel* statusChipHardware = nullptr; // 算力与 AI 引擎状态
    QLabel* statusChipMethod = nullptr; // 当前算法模式指示灯
    QLabel* statusChipGrid = nullptr; // 当前计算网格精细度
    QLabel* statusChipData = nullptr; // 内存地震数据规格

    enum DataSourceType {
        SourceExternal = 0,    // 外部导入实测数据 (打开/拖拽 SEGY)
        SourceSynthetic2L = 1, // 内部正演合成: 两层模型
        SourceSynthetic3L = 2  // 内部正演合成: 三层模型
    };
    DataSourceType m_dataSourceType = SourceExternal; // 默认标记为外部数据

    protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;
};