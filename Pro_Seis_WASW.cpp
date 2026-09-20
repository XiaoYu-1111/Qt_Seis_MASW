#include "Pro_Seis_WASW.h"
#include "ui_Pro_Seis_WASW.h"
#include "qcustomplot.h"
#include "style.h"

// 引入模块
#include "Pro_h/SeismicIO.h"
#include "Pro_h/SeismicView2D.h"
#include "Pro_h/SignalProcessingUtils.h"

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

// 辅助函数：根据名称生成 QCustomPlot 色标渐变
static QCPColorGradient getScientificGradient(const QString& type, bool invert = false)
{
    QCPColorGradient grad;
    grad.clearColorStops();

    if (type == "Jet") {
        grad = QCPColorGradient::gpJet;
    }
    else if (type == "Turbo") {
        grad.setColorStopAt(0.0, QColor(48, 18, 59));
        grad.setColorStopAt(0.1, QColor(70, 107, 227));
        grad.setColorStopAt(0.2, QColor(40, 187, 235));
        grad.setColorStopAt(0.35, QColor(50, 241, 151));
        grad.setColorStopAt(0.5, QColor(164, 252, 60));
        grad.setColorStopAt(0.7, QColor(237, 208, 58));
        grad.setColorStopAt(0.85, QColor(253, 128, 40));
        grad.setColorStopAt(1.0, QColor(122, 4, 3));
    }
    else if (type == "Viridis") {
        grad.setColorStopAt(0.0, QColor(68, 1, 84));
        grad.setColorStopAt(0.2, QColor(72, 35, 116));
        grad.setColorStopAt(0.4, QColor(64, 103, 138));
        grad.setColorStopAt(0.6, QColor(53, 183, 121));
        grad.setColorStopAt(0.8, QColor(143, 215, 68));
        grad.setColorStopAt(1.0, QColor(253, 231, 37));
    }
    else if (type == "Plasma") {
        grad.setColorStopAt(0.0, QColor(13, 8, 135));
        grad.setColorStopAt(0.25, QColor(106, 0, 168));
        grad.setColorStopAt(0.5, QColor(187, 55, 84));
        grad.setColorStopAt(0.75, QColor(249, 142, 9));
        grad.setColorStopAt(1.0, QColor(240, 249, 33));
    }
    else if (type == "Inferno") {
        grad.setColorStopAt(0.0, QColor(0, 0, 4));
        grad.setColorStopAt(0.25, QColor(87, 16, 109));
        grad.setColorStopAt(0.5, QColor(187, 55, 84));
        grad.setColorStopAt(0.75, QColor(249, 142, 9));
        grad.setColorStopAt(1.0, QColor(252, 255, 164));
    }
    else if (type == "Hot") {
        grad = QCPColorGradient::gpHot;
    }
    else if (type == "Grayscale") {
        grad.setColorStopAt(0.0, Qt::black);
        grad.setColorStopAt(1.0, Qt::white);
    }
    else if (type == "Seismic") {
        grad.setColorStopAt(0.0, QColor(0, 0, 180));
        grad.setColorStopAt(0.5, Qt::white);
        grad.setColorStopAt(1.0, QColor(180, 0, 0));
    }
    else {
        grad = QCPColorGradient::gpJet;
    }

    // 反转颜色
    if (invert) {
        QMap<double, QColor> currentStops = grad.colorStops();
        grad.clearColorStops();
        for (auto it = currentStops.constBegin(); it != currentStops.constEnd(); ++it) {
            grad.setColorStopAt(1.0 - it.key(), it.value());
        }
    }
    return grad;
}

Pro_Seis_WASW::Pro_Seis_WASW(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::Pro_Seis_WASWClass)
{
    ui->setupUi(this);

    this->setWindowTitle(QStringLiteral("SeisTool-WASW 面波频散分析系统 v1.0"));
    this->setMinimumSize(1100,680);
    this->setStyleSheet(StyleHelper::getDarkScientificStyle());
    setWindowIcon(QIcon(":/Pro_Seis_WASW/icon/layer.png"));
    resize(1366,800);

    createActionsAndToolBars(); // 建立工具栏和打开按钮
    initUI();
    initControlDock();
    initLogDock();

    statusBar()->showMessage(QStringLiteral("就绪 (Ready)"));
}

Pro_Seis_WASW::~Pro_Seis_WASW()
{
    delete ui;
}

void Pro_Seis_WASW::createActionsAndToolBars()
{
    // =========================================================
    // 方案：直接在工具栏嵌入带 #btnPrimary 样式的标准 QPushButton
    // =========================================================
    QPushButton* btnOpen = new QPushButton(QStringLiteral("📁 打开 SEGY 数据"), this);
    btnOpen->setObjectName("btnPrimary");             // 对应我们在 style.h 中定义的蓝底样式
    btnOpen->setCursor(Qt::PointingHandCursor);       // 鼠标悬停显示“小手”手势
    btnOpen->setMinimumHeight(32);                    // 设置适中的高度
    btnOpen->setStyleSheet(
        "QPushButton#btnPrimary {"
        "   background-color: #0284c7;"
        "   border: 1px solid #0369a1;"
        "   color: #ffffff;"
        "   font-weight: bold;"
        "   border-radius: 4px;"
        "   padding: 4px 16px;"
        "}"
        "QPushButton#btnPrimary:hover {"
        "   background-color: #0369a1;"
        "}"
        "QPushButton#btnPrimary:pressed {"
        "   background-color: #0c4a6e;"
        "}"
    );

    // 绑定快捷键 Ctrl+O，方便键盘操作
    btnOpen->setShortcut(QKeySequence::Open);
    btnOpen->setToolTip(QStringLiteral("打开并载入 SEGY 地震道集数据 (Ctrl+O)"));

    // 点击直接触发载入
    connect(btnOpen, &QPushButton::clicked, this, &Pro_Seis_WASW::onOpenSegy);

    // 将蓝底按钮加入工具栏
    ui->mainToolBar->addWidget(btnOpen);
}

void Pro_Seis_WASW::initUI()
{
    mainTabWidget = new QTabWidget(this);
    setCentralWidget(mainTabWidget);

    initMainTabs();
}

void Pro_Seis_WASW::initMainTabs()
{
    // --- 页面 1: 原始道集容器 (保持不变) ---
    seismicViewContainer = new QWidget(this);
    QVBoxLayout* contLayout = new QVBoxLayout(seismicViewContainer);
    contLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* emptyLabel = new QLabel(QStringLiteral("请点击顶部【打开 SEGY 数据】载入地震剖面"), seismicViewContainer);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color: #64748b; font-size: 16px;");
    contLayout->addWidget(emptyLabel);

    // =========================================================
    // --- 页面 2: 频散能量谱 (容器 + 画布 + 底部控制栏) ---
    // =========================================================
    dispersionContainer = new QWidget(this);
    QVBoxLayout* dispMainLayout = new QVBoxLayout(dispersionContainer);
    dispMainLayout->setContentsMargins(0, 0, 0, 0);
    dispMainLayout->setSpacing(0);

    // 1. 初始化 QCustomPlot (纯白科研背景)
    plotDispersion = new QCustomPlot(dispersionContainer);
    dispMainLayout->addWidget(plotDispersion, 1); // 权重 1

    plotDispersion->setBackground(Qt::white);
    plotDispersion->axisRect()->setBackground(Qt::white);

    QFont labelFont("Segoe UI", 10, QFont::Bold);
    QFont tickFont("Segoe UI", 9);
    QList<QCPAxis*> axes = { plotDispersion->xAxis, plotDispersion->yAxis,
                             plotDispersion->xAxis2, plotDispersion->yAxis2 };

    for (QCPAxis* axis : axes) {
        axis->setBasePen(QPen(Qt::black, 1.2));
        axis->setTickPen(QPen(Qt::black, 1.2));
        axis->setSubTickPen(QPen(Qt::black, 1.0));
        axis->setTickLabelColor(Qt::black);
        axis->setLabelColor(Qt::black);
        axis->setLabelFont(labelFont);
        axis->setTickLabelFont(tickFont);
    }
    plotDispersion->xAxis2->setVisible(true);
    plotDispersion->xAxis2->setTicks(false);
    plotDispersion->xAxis2->setTickLabels(false);
    plotDispersion->yAxis2->setVisible(true);
    plotDispersion->yAxis2->setTicks(false);
    plotDispersion->yAxis2->setTickLabels(false);

    plotDispersion->xAxis->setLabel(QStringLiteral("频率 Frequency (Hz)"));
    plotDispersion->yAxis->setLabel(QStringLiteral("相速度 Phase Velocity (m/s)"));
    plotDispersion->xAxis->grid()->setPen(QPen(QColor(235, 235, 235), 1, Qt::SolidLine));
    plotDispersion->yAxis->grid()->setPen(QPen(QColor(235, 235, 235), 1, Qt::SolidLine));
    plotDispersion->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);


    // =========================================================
    // 5. 开启并绑定右键菜单 (Context Menu)
    // =========================================================
    plotDispersion->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(plotDispersion, &QCustomPlot::customContextMenuRequested, this, [=](const QPoint& pos) {
        QMenu menu(this);

        // --- 动作 1: 重置视图 (Reset View) ---
        QAction* actReset = menu.addAction(QStringLiteral("重置视图 (Reset View)"));
        actReset->setIcon(QIcon::fromTheme("view-refresh")); // 若有系统图标则显示
        connect(actReset, &QAction::triggered, this, [=]() {
            // 如果已经计算过，严格重置为当前的 Fmin~Fmax 和 Vmin~Vmax
            if (m_dispFmax > m_dispFmin && m_dispVmax > m_dispVmin) {
                plotDispersion->xAxis->setRange(m_dispFmin, m_dispFmax);
                plotDispersion->yAxis->setRange(m_dispVmin, m_dispVmax);
            }
            else {
                plotDispersion->rescaleAxes();
            }
            plotDispersion->replot();
            });

        menu.addSeparator();

        // --- 动作 2: 导出当前频散谱高清图片 (Save PNG) ---
        QAction* actSave = menu.addAction(QStringLiteral("保存图片 (Export PNG)..."));
        connect(actSave, &QAction::triggered, this, [=]() {
            QString defaultName = QString("Dispersion_%1.png")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            QString fileName = QFileDialog::getSaveFileName(
                this, QStringLiteral("保存频散谱图像"), defaultName, QStringLiteral("PNG 图片 (*.png);;所有文件 (*.*)"));
            if (!fileName.isEmpty()) {
                // scale=2.0 导出 2 倍高分辨率图片，线条文字极其清晰
                plotDispersion->savePng(fileName, 0, 0, 2.0, 100);
            }
            });
        // =========================================================
        // --- 动作 3: 新增【导出为 SEGY 数据】---
        // =========================================================
        QAction* actExportSegy = menu.addAction(QStringLiteral("💾 导出频散能量谱为 SEGY (*.sgy)..."));
        connect(actExportSegy, &QAction::triggered, this, &Pro_Seis_WASW::exportDispersionToSegy);

        menu.exec(plotDispersion->mapToGlobal(pos));
        // 在鼠标点击处弹出菜单
        menu.exec(plotDispersion->mapToGlobal(pos));
        });

    // 2. 初始化底部控制栏 (Bottom Bar)
    QFrame* dispBottomBar = new QFrame(dispersionContainer);
    dispBottomBar->setFixedHeight(45);
    dispBottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QHBoxLayout* barLayout = new QHBoxLayout(dispBottomBar);
    barLayout->setContentsMargins(15, 0, 15, 0);
    barLayout->setSpacing(12);

    // 状态显示（鼠标当前坐标和能量值）
    lblDispStatus = new QLabel(QStringLiteral("就绪 (Ready)"), dispBottomBar);
    lblDispStatus->setStyleSheet("color: #38bdf8; font-family: Consolas; font-size: 12px; border: none;");

    // 1. 归一化模式下拉框
    QLabel* lblNorm = new QLabel(QStringLiteral("归一化:"), dispBottomBar);
    lblNorm->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    comboNormMode = new QComboBox(dispBottomBar);
    comboNormMode->setFixedWidth(150);
    comboNormMode->addItem(QStringLiteral("按频率归一化 (每列)")); // 索引 0: 默认推荐
    comboNormMode->addItem(QStringLiteral("全局归一化"));           // 索引 1
    comboNormMode->setCurrentIndex(1);

    // 绑定信号：切换模式时瞬间重绘，无需重新计算算法！
    connect(comboNormMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &Pro_Seis_WASW::renderDispersionMap);
    // 色标切换控件
    QLabel* lblCmap = new QLabel(QStringLiteral("色标 (Colormap):"), dispBottomBar);
    lblCmap->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    comboDispCmap = new QComboBox(dispBottomBar);
    comboDispCmap->setFixedWidth(110);
    comboDispCmap->addItems({ "Jet", "Turbo", "Viridis", "Plasma", "Inferno", "Hot", "Seismic", "Grayscale" });

    chkDispInv = new QCheckBox(QStringLiteral("反转 (Inv)"), dispBottomBar);
    chkDispInv->setStyleSheet("color: #cbd5e1;");

    barLayout->addWidget(lblDispStatus);
    barLayout->addStretch();
    barLayout->addWidget(lblNorm);          // <--- 新增归一化标签
    barLayout->addWidget(comboNormMode);     // <--- 新增归一化选择框
    barLayout->addWidget(lblCmap);
    barLayout->addWidget(comboDispCmap);
    barLayout->addWidget(chkDispInv);

    dispMainLayout->addWidget(dispBottomBar, 0); // 权重 0，固定在底部

    // 3. 信号绑定
    connect(comboDispCmap, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Pro_Seis_WASW::updateDispersionColormap);
    connect(chkDispInv, &QCheckBox::toggled, this, &Pro_Seis_WASW::updateDispersionColormap);

    // 4. 鼠标滑动动态读取 (频率, 速度, 归一化能量)
    connect(plotDispersion, &QCustomPlot::mouseMove, this, [=](QMouseEvent* e) {
        if (!dispColorMap || !dispColorMap->data()) return;

        double f = plotDispersion->xAxis->pixelToCoord(e->pos().x());
        double v = plotDispersion->yAxis->pixelToCoord(e->pos().y());

        // 判断鼠标是否在坐标系有效可视区内
        if (plotDispersion->axisRect()->rect().contains(e->pos())) {
            int keyIndex = 0, valIndex = 0;
            dispColorMap->data()->coordToCell(f, v, &keyIndex, &valIndex);

            double energy = 0.0;
            if (keyIndex >= 0 && keyIndex < dispColorMap->data()->keySize() &&
                valIndex >= 0 && valIndex < dispColorMap->data()->valueSize()) {
                energy = dispColorMap->data()->cell(keyIndex, valIndex);
            }
            lblDispStatus->setText(QString("Freq: %1 Hz | Vel: %2 m/s | Energy: %3")
                .arg(f, 0, 'f', 2).arg(v, 0, 'f', 1).arg(energy, 0, 'f', 4));
        }
        });

    // 在 initMainTabs() 中，找到 dispBottomBar 的布局部分，追加以下代码：

    // 1. 拾取控制组件
    QFrame* vline = new QFrame(dispBottomBar);
    vline->setFrameShape(QFrame::VLine);
    vline->setStyleSheet("color: #475569;");

    chkPickMode = new QCheckBox(QStringLiteral("拾取模式"), dispBottomBar);
    chkPickMode->setStyleSheet("color: #38bdf8; font-weight: bold;");

    btnUndoPick = new QPushButton(QStringLiteral("撤销点"), dispBottomBar);
    btnUndoPick->setFixedHeight(26);

    btnClearPick = new QPushButton(QStringLiteral("清空曲线"), dispBottomBar);
    btnClearPick->setFixedHeight(26);

    btnExportCurve = new QPushButton(QStringLiteral("导出曲线..."), dispBottomBar);
    btnExportCurve->setFixedHeight(26);

    // 将新控件装入底栏
    barLayout->addWidget(vline);
    barLayout->addWidget(chkPickMode);
    barLayout->addWidget(btnUndoPick);
    barLayout->addWidget(btnClearPick);
    barLayout->addWidget(btnExportCurve);

    // 2. 拾取模式切换信号 (开启拾取时，暂停鼠标滚轮平移以防拖拽干扰)
    connect(chkPickMode, &QCheckBox::toggled, this, [=](bool checked) {
        if (checked) {
            plotDispersion->setInteractions(QCP::iRangeZoom); // 仅保留缩放，禁用拖拽
            lblDispStatus->setText(QStringLiteral("【拾取开启】在热力图能量带上左键点击，算法将自动吸附极大值峰值"));
        }
        else {
            plotDispersion->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
            lblDispStatus->setText(QStringLiteral("就绪 (Ready)"));
        }
        });

    // 3. 按钮动作连接
    connect(btnUndoPick, &QPushButton::clicked, this, [=]() {
        if (!m_pickedPoints.isEmpty()) {
            m_pickedPoints.removeLast();
            updatePickVisuals();
        }
        });

    connect(btnClearPick, &QPushButton::clicked, this, [=]() {
        m_pickedPoints.clear();
        updatePickVisuals();
        });

    connect(btnExportCurve, &QPushButton::clicked, this, &Pro_Seis_WASW::exportPickedCurve);

    // 4. 监听热力图点击事件
    connect(plotDispersion, &QCustomPlot::mousePress, this, &Pro_Seis_WASW::onDispersionPlotClicked);

    // --- 页面 3: 提取的频散曲线对比 ---
    plotCurve1D = new Data_show::Plot1D(this);
    plotCurve1D->m_setName(QStringLiteral("频散曲线对比 (Dispersion Curves)"));

    // 将整个容器加入 Tab 2
    mainTabWidget->addTab(seismicViewContainer, QStringLiteral("1. 原始道集 (Shot Gather)"));
    mainTabWidget->addTab(dispersionContainer, QStringLiteral("2. 频散能量谱 (Dispersion Map)"));
    mainTabWidget->addTab(plotCurve1D, QStringLiteral("3. 频散曲线 (Extracted Curves)"));
}

void Pro_Seis_WASW::initControlDock()
{
    controlDock = new QDockWidget(QStringLiteral("参数控制面板"), this);
    controlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* dockContainer = new QWidget(controlDock);
    QVBoxLayout* dockLayout = new QVBoxLayout(dockContainer);
    dockLayout->setContentsMargins(10, 10, 10, 10);
    dockLayout->setSpacing(10);

    // ==========================================
    // 组 1: 采集与观测系统几何参数 (Geometry)
    // ==========================================
    QGroupBox* geomGroup = new QGroupBox(QStringLiteral("观测系统参数 (Geometry)"), dockContainer);
    QFormLayout* geomLayout = new QFormLayout(geomGroup);

    spinDt = new QDoubleSpinBox(geomGroup);
    spinDt->setRange(0.001, 100.0);
    spinDt->setValue(1);      // 默认 0.5 ms (对应你的 MATLAB 数据)
    spinDt->setDecimals(3);
    spinDt->setSuffix(" ms");
    spinDt->setToolTip(QStringLiteral("时间采样率 dt，打开 SEGY 时会自动从道头读取"));

    spinDx = new QDoubleSpinBox(geomGroup);
    spinDx->setRange(0.01, 500.0);
    spinDx->setValue(1);      // 对应你的 MATLAB 数据 trece_offset = 0.2
    spinDx->setDecimals(2);
    spinDx->setSuffix(" m");

    spinOffset0 = new QDoubleSpinBox(geomGroup);
    spinOffset0->setRange(0.0, 1000.0);
    spinOffset0->setValue(0.0); // 最小炮检距
    spinOffset0->setSuffix(" m");

    geomLayout->addRow(QStringLiteral("采样间隔 (dt):"), spinDt);
    geomLayout->addRow(QStringLiteral("道间距 (dx):"), spinDx);
    geomLayout->addRow(QStringLiteral("最小炮检距 (x0):"), spinOffset0);
    dockLayout->addWidget(geomGroup);

    // ==========================================
    // 组 2: 频散计算扫描网格 (Dispersion Grid)
    // ==========================================
    QGroupBox* scanGroup = new QGroupBox(QStringLiteral("频散扫描范围 (Scan Range)"), dockContainer);
    QFormLayout* scanLayout = new QFormLayout(scanGroup);

    comboMethod = new QComboBox(scanGroup);
    comboMethod->addItems({ QStringLiteral("移相法 (Phase Shift)"), QStringLiteral("F-K 变换法") });

    spinFmin = new QDoubleSpinBox(scanGroup);
    spinFmin->setRange(0.1, 500.0);
    spinFmin->setValue(5.0);    // 对应你的 MATLAB: 5 Hz
    spinFmin->setSuffix(" Hz");

    spinFreqMax = new QDoubleSpinBox(scanGroup);
    spinFreqMax->setRange(1.0, 1000.0);
    spinFreqMax->setValue(80.0); // 对应你的 MATLAB: 80 Hz
    spinFreqMax->setSuffix(" Hz");

    spinVmin = new QDoubleSpinBox(scanGroup);
    spinVmin->setRange(10.0, 5000.0);
    spinVmin->setValue(400.0);  // 对应你的 MATLAB: 100 m/s
    spinVmin->setSuffix(" m/s");

    spinVmax = new QDoubleSpinBox(scanGroup);
    spinVmax->setRange(50.0, 8000.0);
    spinVmax->setValue(1000.0);  // 对应你的 MATLAB: 700 m/s
    spinVmax->setSuffix(" m/s");

    comboGridQuality = new QComboBox(scanGroup);
    // 使用 QPoint(nf, nv) 作为 Item 的关联数据，x 存 nf，y 存 nv
    comboGridQuality->addItem(QStringLiteral("快速预览 (100 × 100 - 极速)"), QPoint(100, 100));
    comboGridQuality->addItem(QStringLiteral("标准质量 (180 × 250 - 推荐)"), QPoint(180, 250));
    comboGridQuality->addItem(QStringLiteral("高精度 (300 × 500 - 精细)"), QPoint(300, 500));

    // 默认选择【标准质量】
    comboGridQuality->setCurrentIndex(1);

    // 将控件加入布局（建议放在分析方法下方）
    scanLayout->addRow(QStringLiteral("计算方法:"), comboMethod);
    scanLayout->addRow(QStringLiteral("网格质量:"), comboGridQuality); // <--- 新增此行
    scanLayout->addRow(QStringLiteral("频率下限 (Fmin):"), spinFmin);
    scanLayout->addRow(QStringLiteral("频率上限 (Fmax):"), spinFreqMax);
    scanLayout->addRow(QStringLiteral("速度下限 (Vmin):"), spinVmin);
    scanLayout->addRow(QStringLiteral("速度上限 (Vmax):"), spinVmax);
    dockLayout->addWidget(scanGroup);

    // 计算执行按钮
    btnCalculate = new QPushButton(QStringLiteral("开始计算频散谱"), dockContainer);
    btnCalculate->setObjectName("btnPrimary");
    btnCalculate->setMinimumHeight(40);
    connect(btnCalculate, &QPushButton::clicked, this, &Pro_Seis_WASW::onCalculateClicked);
    dockLayout->addWidget(btnCalculate);

    dockLayout->addStretch();
    controlDock->setWidget(dockContainer);
    addDockWidget(Qt::LeftDockWidgetArea, controlDock);
}

void Pro_Seis_WASW::initLogDock()
{
    logDock = new QDockWidget(QStringLiteral("运行日志 (Console Log)"), this);
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea);

    textLog = new QTextEdit(logDock);
    textLog->setReadOnly(true);

    // 1. 去掉 QTextEdit 默认的 QFrame 3D 白边
    textLog->setFrameShape(QFrame::NoFrame);

    // 2. 赋予与深色主题完美融合的背景和文字样式
    textLog->setStyleSheet(
        "QTextEdit {"
        "   background-color: #0b1120;"        // 比主面板稍深的高级控制台底色
        "   color: #cbd5e1;"                    // 柔和的浅灰字
        "   border: none;"                      // 彻底清除边框白线
        "   font-family: 'Consolas', monospace;"
        "   font-size: 13px;"
        "   padding: 4px;"
        "}"
    );

    QString currentTime = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    textLog->append(QStringLiteral("[%1] 系统初始化完成，准备就绪。").arg(currentTime));

    logDock->setWidget(textLog);
    logDock->setMaximumHeight(160);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
}

// =========================================================
// 读取 SEGY 并嵌入显示
// =========================================================
void Pro_Seis_WASW::onOpenSegy()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 SEGY 文件"),
        QString(),
        QStringLiteral("SEGY 地震数据 (*.sgy *.segy *.dat);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    textLog->append(QStringLiteral("[%1] 正在读取文件: %2").arg(timeStr).arg(fileName));

    // 1. 调用模块读取数据
    m_seismicData = SeismicIO::readSegyFile2D(fileName.toStdString());

    if (m_seismicData.empty() || m_seismicData[0].empty()) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("SEGY 读取失败或数据为空！"));
        return;
    }

    int traces = m_seismicData.size();
    int samples = m_seismicData[0].size();

    textLog->append(QStringLiteral("[%1] 读取成功! 总道数: %2, 每道采样点数: %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
        .arg(traces)
        .arg(samples));

    // 2. 调用 SeismicView2D 模块生成视图组件并嵌入 Tab 1
    QWidget* seismicPlotWidget = createSeismicView(m_seismicData, QStringLiteral("道集剖面"), seismicViewContainer);

    // 清空页面 1 原有内容并填入新视图
    QLayout* layout = seismicViewContainer->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    layout->addWidget(seismicPlotWidget);

    // 切换到第 1 页
    mainTabWidget->setCurrentIndex(0);
}

void Pro_Seis_WASW::onCalculateClicked()
{
    if (m_seismicData.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载地震道集数据！"));
        return;
    }

    // 1. 读取参数并缓存
    QPoint gridRes = comboGridQuality->currentData().toPoint();
    m_dispNf = gridRes.x();
    m_dispNv = gridRes.y();

    float dt = static_cast<float>(spinDt->value() / 1000.0);
    float dx = static_cast<float>(spinDx->value());
    float x0 = static_cast<float>(spinOffset0->value());
    m_dispFmin = static_cast<float>(spinFmin->value());
    m_dispFmax = static_cast<float>(spinFreqMax->value());
    m_dispVmin = static_cast<float>(spinVmin->value());
    m_dispVmax = static_cast<float>(spinVmax->value());

    textLog->append(QStringLiteral("[%1] 开始频散谱计算...")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // 2. 计算并将原始能量缓存到 m_rawDispersionEnergy
    m_rawDispersionEnergy = computePhaseShiftDispersion(
        m_seismicData, dt, dx, x0, m_dispFmin, m_dispFmax, m_dispNf, m_dispVmin, m_dispVmax, m_dispNv);

    QApplication::restoreOverrideCursor();

    if (m_rawDispersionEnergy.empty()) {
        textLog->append(QStringLiteral("[%1] 计算失败！").arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
        return;
    }

    // 3. 直接调用渲染函数成图
    renderDispersionMap();

    mainTabWidget->setCurrentWidget(dispersionContainer);

    textLog->append(QStringLiteral("[%1] 频散谱计算完成并已显示。")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
}

void Pro_Seis_WASW::updateDispersionColormap()
{
    if (!dispColorMap) return;

    QString cmapName = comboDispCmap->currentText();
    bool inv = chkDispInv->isChecked();

    QCPColorGradient grad = getScientificGradient(cmapName, inv);

    dispColorMap->setGradient(grad);
    plotDispersion->replot();
}

void Pro_Seis_WASW::renderDispersionMap()
{
    if (m_rawDispersionEnergy.empty() || m_dispNf <= 0 || m_dispNv <= 0) return;

    // 拷贝一份原始矩阵进行处理
    auto processedEnergy = m_rawDispersionEnergy;
    int mode = comboNormMode->currentIndex(); // 0: 按频率归一化, 1: 全局归一化

    if (mode == 0) {
        // ==========================================
        // 模式 A: 按频率 (每列) 归一化 (每列最大值 = 1.0)
        // ==========================================
        for (int fi = 0; fi < m_dispNf; ++fi) {
            float colMax = 0.0f;
            for (int vi = 0; vi < m_dispNv; ++vi) {
                colMax = std::max(colMax, processedEnergy[vi][fi]);
            }
            if (colMax > 1e-6f) {
                for (int vi = 0; vi < m_dispNv; ++vi) {
                    processedEnergy[vi][fi] /= colMax;
                }
            }
        }
    }
    else {
        // ==========================================
        // 模式 B: 全局归一化 (全图最大值 = 1.0)
        // ==========================================
        float globalMax = 0.0f;
        for (int vi = 0; vi < m_dispNv; ++vi) {
            for (int fi = 0; fi < m_dispNf; ++fi) {
                globalMax = std::max(globalMax, processedEnergy[vi][fi]);
            }
        }
        if (globalMax > 1e-6f) {
            for (int vi = 0; vi < m_dispNv; ++vi) {
                for (int fi = 0; fi < m_dispNf; ++fi) {
                    processedEnergy[vi][fi] /= globalMax;
                }
            }
        }
    }

    // 将归一化后的数据写入 QCPColorMap
    plotDispersion->clearPlottables();

    dispColorMap = new QCPColorMap(plotDispersion->xAxis, plotDispersion->yAxis);
    dispColorMap->data()->setSize(m_dispNf, m_dispNv);
    dispColorMap->data()->setRange(QCPRange(m_dispFmin, m_dispFmax), QCPRange(m_dispVmin, m_dispVmax));

    for (int fi = 0; fi < m_dispNf; ++fi) {
        for (int vi = 0; vi < m_dispNv; ++vi) {
            dispColorMap->data()->setCell(fi, vi, processedEnergy[vi][fi]);
        }
    }

    // 挂接或更新色标条
    if (plotDispersion->plotLayout()->elementCount() > 1) {
        dispColorScale = qobject_cast<QCPColorScale*>(plotDispersion->plotLayout()->element(0, 1));
    }
    if (!dispColorScale) {
        dispColorScale = new QCPColorScale(plotDispersion);
        plotDispersion->plotLayout()->addElement(0, 1, dispColorScale);
        dispColorScale->setType(QCPAxis::atRight);
        dispColorScale->axis()->setLabel(QStringLiteral("归一化能量 (Normalized Energy)"));
        dispColorScale->axis()->setLabelColor(Qt::black);
        dispColorScale->axis()->setTickLabelColor(Qt::black);
        dispColorScale->axis()->setBasePen(QPen(Qt::black, 1.2));
        dispColorScale->axis()->setTickPen(QPen(Qt::black, 1.2));
    }

    // 关键：强制锁定为 0.0 ~ 1.0
    dispColorMap->setColorScale(dispColorScale);
    dispColorMap->setDataRange(QCPRange(0.0, 1.0));
    dispColorScale->setDataRange(QCPRange(0.0, 1.0));
    dispColorMap->setInterpolate(true);

    // 刷新色标渐变并重绘
    updateDispersionColormap();
    plotDispersion->xAxis->setRange(m_dispFmin, m_dispFmax);
    plotDispersion->yAxis->setRange(m_dispVmin, m_dispVmax);

    // 重新挂接高亮拾取图层 (确保始终浮在热力图之上)
    pickGraphOnDispersion = plotDispersion->addGraph();
    pickGraphOnDispersion->setPen(QPen(QColor(255, 230, 0), 2.5)); // 荧光黄色连线
    pickGraphOnDispersion->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(255, 40, 40), Qt::white, 7)); // 红白圆点

    // 恢复已有数据
    updatePickVisuals();

    plotDispersion->replot();
}

void Pro_Seis_WASW::onDispersionPlotClicked(QMouseEvent* event)
{
    // 未开启拾取模式、未计算数据或非左键点击，直接跳过
    if (!chkPickMode || !chkPickMode->isChecked()) return;
    if (m_rawDispersionEnergy.empty() || m_dispNf <= 0 || m_dispNv <= 0) return;
    if (event->button() != Qt::LeftButton) return;

    // 判断指针及点击位置是否在有效坐标区域内
    if (!plotDispersion || !plotDispersion->axisRect() ||
        !plotDispersion->axisRect()->rect().contains(event->pos())) return;

    // 1. 将鼠标点击像素转换为物理坐标 (Hz, m/s)
    double f_click = plotDispersion->xAxis->pixelToCoord(event->pos().x());
    double v_click = plotDispersion->yAxis->pixelToCoord(event->pos().y());

    double df = (m_dispFmax - m_dispFmin) / std::max(1, m_dispNf - 1);
    double dv = (m_dispVmax - m_dispVmin) / std::max(1, m_dispNv - 1);

    // 找到最近的频率列索引和速度索引
    int fi = std::clamp((int)std::round((f_click - m_dispFmin) / df), 0, m_dispNf - 1);
    int vi_click = std::clamp((int)std::round((v_click - m_dispVmin) / dv), 0, m_dispNv - 1);

    // =========================================================
    // 2. 局部极大值自动吸附 (带双重下标安全保护)
    // =========================================================
    int searchRadius = std::max(6, m_dispNv / 20);
    int vi_start = std::max(0, vi_click - searchRadius);
    int vi_end = std::min(m_dispNv - 1, vi_click + searchRadius);

    int best_vi = vi_click;
    float max_energy = -1.0f;

    int totalRows = static_cast<int>(m_rawDispersionEnergy.size());
    for (int vi = vi_start; vi <= vi_end; ++vi) {
        if (vi >= 0 && vi < totalRows) {
            int totalCols = static_cast<int>(m_rawDispersionEnergy[vi].size());
            if (fi >= 0 && fi < totalCols) {
                if (m_rawDispersionEnergy[vi][fi] > max_energy) {
                    max_energy = m_rawDispersionEnergy[vi][fi];
                    best_vi = vi;
                }
            }
        }
    }

    double snapped_f = m_dispFmin + fi * df;
    double snapped_v = m_dispVmin + best_vi * dv;

    // 3. 插入或更新点集
    bool replaced = false;
    for (int i = 0; i < m_pickedPoints.size(); ++i) {
        if (std::abs(m_pickedPoints[i].x() - snapped_f) < df * 0.7) {
            m_pickedPoints[i] = QPointF(snapped_f, snapped_v);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_pickedPoints.append(QPointF(snapped_f, snapped_v));
        std::sort(m_pickedPoints.begin(), m_pickedPoints.end(), [](const QPointF& a, const QPointF& b) {
            return a.x() < b.x();
            });
    }

    // 4. 安全刷新两处视图
    updatePickVisuals();
}

void Pro_Seis_WASW::updatePickVisuals()
{
    QVector<double> qf, qv;
    std::vector<double> std_f, std_v;

    qf.reserve(m_pickedPoints.size());
    qv.reserve(m_pickedPoints.size());
    std_f.reserve(m_pickedPoints.size());
    std_v.reserve(m_pickedPoints.size());

    for (const auto& pt : m_pickedPoints) {
        qf.append(pt.x());
        qv.append(pt.y());
        std_f.push_back(pt.x());
        std_v.push_back(pt.y());
    }

    // --- 1. 更新 Tab 2 热力图上的叠加曲线 ---
    // 动态防御：如果图层意外为空或被清除了，自动重新创建
    if (!pickGraphOnDispersion || plotDispersion->graphCount() == 0) {
        pickGraphOnDispersion = plotDispersion->addGraph();
        pickGraphOnDispersion->setPen(QPen(QColor(255, 230, 0), 2.5)); // 荧光黄
        pickGraphOnDispersion->setScatterStyle(
            QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(255, 40, 40), Qt::white, 7)); // 红白圆点
    }
    pickGraphOnDispersion->setData(qf, qv);
    plotDispersion->replot();

    // --- 2. 实时同步到 Tab 3 (Plot1D 期刊级展示) ---
    if (plotCurve1D) {
        if (!std_f.empty()) {
            plotCurve1D->setData(std_f, std_v, QStringLiteral("拾取的基阶频散曲线 (Fundamental Mode)"));
            plotCurve1D->xAxis->setLabel(QStringLiteral("频率 Frequency (Hz)"));
            plotCurve1D->yAxis->setLabel(QStringLiteral("相速度 Phase Velocity (m/s)"));

            // 特殊保护：当刚拾取第 1 个点时，手动撑开视野范围，防止范围为 0 导致崩溃
            if (std_f.size() == 1) {
                plotCurve1D->xAxis->setRange(std_f[0] - 5.0, std_f[0] + 5.0);
                plotCurve1D->yAxis->setRange(std_v[0] - 50.0, std_v[0] + 50.0);
                plotCurve1D->replot();
            }
        }
        else {
            // 【核心修复】：千万不要调用 clearGraphs()！只需清空数据即可保持指针有效！
            if (plotCurve1D->graphCount() > 0) {
                plotCurve1D->graph(0)->data()->clear();
                plotCurve1D->replot();
            }
        }
    }

    // 状态栏提示
    if (lblDispStatus) {
        lblDispStatus->setText(QString("已拾取点数: %1 | 频段覆盖: [%2 ~ %3] Hz")
            .arg(m_pickedPoints.size())
            .arg(m_pickedPoints.isEmpty() ? 0.0 : m_pickedPoints.first().x(), 0, 'f', 1)
            .arg(m_pickedPoints.isEmpty() ? 0.0 : m_pickedPoints.last().x(), 0, 'f', 1));
    }
}

void Pro_Seis_WASW::exportPickedCurve()
{
    if (m_pickedPoints.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前暂无拾取的频散曲线点！"));
        return;
    }

    QString defaultName = QString("DispersionCurve_%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出频散曲线"), defaultName,
        QStringLiteral("标准数据文本 (*.txt);;CSV 表格文件 (*.csv);;所有文件 (*.*)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("无法写入文件: ") + file.errorString());
        return;
    }

    QTextStream out(&file);
    QString sep = fileName.endsWith(".csv", Qt::CaseInsensitive) ? "," : "\t";

    // 写入标准面波反演所需的三列数据：频率、相速度、对应波长 (Lambda = V / F)
    out << "# Frequency(Hz)" << sep << "PhaseVelocity(m/s)" << sep << "Wavelength(m)" << "\n";

    for (const auto& pt : m_pickedPoints) {
        double f = pt.x();
        double v = pt.y();
        double lambda = (f > 0) ? (v / f) : 0.0;

        out << QString::number(f, 'f', 3) << sep
            << QString::number(v, 'f', 2) << sep
            << QString::number(lambda, 'f', 2) << "\n";
    }

    file.close();
    textLog->append(QStringLiteral("[%1] 频散曲线已成功导出: 共 %2 个点至文件 %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
        .arg(m_pickedPoints.size()).arg(fileName));
}



void Pro_Seis_WASW::exportDispersionToSegy()
{
    // 1. 安全检查
    if (m_rawDispersionEnergy.empty() || m_dispNf <= 0 || m_dispNv <= 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前暂无计算好的频散谱数据，请先计算！"));
        return;
    }

    // 2. 获取用户保存路径
    QString defaultName = QString("DispersionMap_F%1-%2Hz_V%3-%4_%5.sgy")
        .arg((int)m_dispFmin).arg((int)m_dispFmax)
        .arg((int)m_dispVmin).arg((int)m_dispVmax)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出频散能量谱为 SEGY"),
        defaultName,
        QStringLiteral("SEGY 地震数据 (*.sgy *.segy);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    // 3. 获取当前界面的归一化数据
    auto processedEnergy = m_rawDispersionEnergy;
    int mode = comboNormMode ? comboNormMode->currentIndex() : 0;

    if (mode == 0) {
        // 按频率（每列）归一化
        for (int fi = 0; fi < m_dispNf; ++fi) {
            float colMax = 0.0f;
            for (int vi = 0; vi < m_dispNv; ++vi) {
                colMax = std::max(colMax, processedEnergy[vi][fi]);
            }
            if (colMax > 1e-6f) {
                for (int vi = 0; vi < m_dispNv; ++vi) {
                    processedEnergy[vi][fi] /= colMax;
                }
            }
        }
    }
    else {
        // 全局归一化
        // 1. 寻找全图最大值 (双层循环)
        float globalMax = 0.0f;
        for (int vi = 0; vi < m_dispNv; ++vi) {
            for (int fi = 0; fi < m_dispNf; ++fi) {
                globalMax = std::max(globalMax, processedEnergy[vi][fi]);
            }
        }

        // 2. 将全图所有元素除以 globalMax (同样需要双层循环！)
        if (globalMax > 1e-6f) {
            for (int vi = 0; vi < m_dispNv; ++vi) {
                for (int fi = 0; fi < m_dispNf; ++fi) { // <--- 补上这层内循环
                    processedEnergy[vi][fi] /= globalMax;
                }
            }
        }
    }

    // =========================================================
    // 4. 矩阵转置：构建 SEGY 格式标准布局
    // 目标布局：[Nf 道][Nv 采样点]
    // 这样在任何地震软件中打开，横轴自动为频率，纵轴自动为相速度
    // =========================================================
    std::vector<std::vector<float>> segyData(m_dispNf, std::vector<float>(m_dispNv, 0.0f));
    for (int fi = 0; fi < m_dispNf; ++fi) {
        for (int vi = 0; vi < m_dispNv; ++vi) {
            segyData[fi][vi] = processedEnergy[vi][fi];
        }
    }

    // 计算速度等效采样间隔 (s)，作为 SEGY 头的 dt 记录
    float dv = (m_dispVmax - m_dispVmin) / std::max(1, m_dispNv - 1);
    float dt_equivalent = dv / 1000.0f; // 缩放保存，防止数值溢出

    // 5. 调用已有 SeismicIO 写入
    SeismicIO::writeSegyFile2D(segyData, fileName.toStdString(), dt_equivalent);

    // 6. 日志与弹窗提示
    QString logMsg = QStringLiteral("[%1] 频散能量谱已成功导出为 SEGY: 总道数(频率)=%2, 每道采样点(速度)=%3 -> %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
        .arg(m_dispNf).arg(m_dispNv).arg(fileName);

    textLog->append(logMsg);
    QMessageBox::information(this, QStringLiteral("导出成功"),
        QStringLiteral("频散能量谱已成功导出为标准 SEGY 文件！\n\n- 总道数 (频率点): %1\n- 每道点数 (速度点): %2\n- 速度范围: %3 ~ %4 m/s\n- 频率范围: %5 ~ %6 Hz")
        .arg(m_dispNf).arg(m_dispNv).arg(m_dispVmin).arg(m_dispVmax).arg(m_dispFmin).arg(m_dispFmax));
}