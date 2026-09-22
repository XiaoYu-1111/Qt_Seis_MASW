#include "Pro_Seis_MASW.h"
#include "ui_Pro_Seis_MASW.h"
#include "qcustomplot.h"
#include "style.h"

// 引入模块
#include "Pro_h/SeismicIO.h"
#include "Pro_h/SeismicView2D.h"
#include "Pro_h/SignalProcessingUtils.h"
#include "Pro_h/RayleighForwardSolver.h"

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

Pro_Seis_MASW::Pro_Seis_MASW(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::Pro_Seis_MASWClass)
{
    ui->setupUi(this);

    this->setWindowTitle(QStringLiteral("SeisTool-MASW 面波频散分析系统 v1.0"));
    this->setMinimumSize(1100,680);
    this->setStyleSheet(StyleHelper::getDarkScientificStyle());
    this->setAcceptDrops(true);
    setWindowIcon(QIcon(":/Pro_Seis_WASW/icon/layer.png"));
    resize(1366,800);

    createActionsAndToolBars(); // 建立工具栏和打开按钮
    initUI();
    initControlDock();
    initLogDock();

    initStatusBar(); // <--- 调用状态栏初始化

    // 默认触发一次初始状态
    statusChipMethod->setText(QStringLiteral("<font color='#38bdf8'>●</font> <b>算法</b>: 移相法"));
    statusChipGrid->setText(QStringLiteral("<font color='#f59e0b'>●</font> <b>网格</b>: 180×250"));
}

Pro_Seis_MASW::~Pro_Seis_MASW()
{
    delete ui;
}

void Pro_Seis_MASW::createActionsAndToolBars()
{
    ui->mainToolBar->setMovable(false);
    ui->mainToolBar->setFixedHeight(44); // 适度加高，更显大气
    ui->mainToolBar->setStyleSheet(
        "QToolBar { background-color: #0f172a; border-bottom: 1px solid #334155; spacing: 8px; padding: 0 10px; }"
    );

    // =========================================================
    // 1. 左侧快捷功能按钮区
    // =========================================================
    // A. 打开数据 (主按钮)
    QPushButton* btnOpen = new QPushButton(QStringLiteral("📁 打开 SEGY 数据"), this);
    btnOpen->setObjectName("btnPrimary");
    btnOpen->setCursor(Qt::PointingHandCursor);
    btnOpen->setMinimumHeight(30);
    btnOpen->setShortcut(QKeySequence::Open);
    btnOpen->setToolTip(QStringLiteral("打开并载入 SEGY 地震道集数据 (Ctrl+O)"));
    btnOpen->setStyleSheet(
        "QPushButton#btnPrimary { background-color: #0284c7; border: 1px solid #0369a1; color: white; font-weight: bold; border-radius: 4px; padding: 4px 14px; }"
        "QPushButton#btnPrimary:hover { background-color: #0369a1; }"
        "QPushButton#btnPrimary:pressed { background-color: #0c4a6e; }"
    );
    connect(btnOpen, &QPushButton::clicked, this, &Pro_Seis_MASW::onOpenSegy);
    ui->mainToolBar->addWidget(btnOpen);

    // 按钮通用次级样式
    QString secBtnStyle =
        "QPushButton { background-color: #1e293b; border: 1px solid #334155; color: #cbd5e1; border-radius: 4px; padding: 4px 12px; font-size: 12px; min-height: 28px; }"
        "QPushButton:hover { background-color: #334155; color: white; border-color: #64748b; }"
        "QPushButton:pressed { background-color: #0f172a; }";

    // B. 全局重置按钮
    QPushButton* btnReset = new QPushButton(QStringLiteral("🔄 重置视图"), this);
    btnReset->setCursor(Qt::PointingHandCursor);
    btnReset->setStyleSheet(secBtnStyle);
    btnReset->setToolTip(QStringLiteral("将所有页面视图、缩放比例与曲线一键复位"));
    connect(btnReset, &QPushButton::clicked, this, &Pro_Seis_MASW::onResetAll);
    ui->mainToolBar->addWidget(btnReset);

    // C. 帮助说明按钮
    QPushButton* btnHelp = new QPushButton(QStringLiteral("📖 帮助指南"), this);
    btnHelp->setCursor(Qt::PointingHandCursor);
    btnHelp->setStyleSheet(secBtnStyle);
    btnHelp->setToolTip(QStringLiteral("查看 MASW 面波分析理论、系统架构与操作快捷键"));
    connect(btnHelp, &QPushButton::clicked, this, &Pro_Seis_MASW::onShowHelp);
    ui->mainToolBar->addWidget(btnHelp);

    // =========================================================
    // 2. 中间弹簧 (把数据看板推到最右侧)
    // =========================================================
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(spacer);

    // =========================================================
    // 3. 右侧数据看板徽章区 (Data Badges)
    // =========================================================
    QString badgeStyle =
        "QLabel {"
        "   background-color: #1e293b;"
        "   border: 1px solid #334155;"
        "   border-radius: 12px;"
        "   padding: 2px 10px;"
        "   color: #94a3b8;"
        "   font-family: 'Consolas', 'Segoe UI', monospace;"
        "   font-size: 12px;"
        "}";

    lblBadgeFile = new QLabel(this); lblBadgeFile->setStyleSheet(badgeStyle);
    lblBadgeTraces = new QLabel(this); lblBadgeTraces->setStyleSheet(badgeStyle);
    lblBadgeDt = new QLabel(this); lblBadgeDt->setStyleSheet(badgeStyle);
    lblBadgeTime = new QLabel(this); lblBadgeTime->setStyleSheet(badgeStyle);

    ui->mainToolBar->addWidget(lblBadgeFile);
    ui->mainToolBar->addWidget(lblBadgeTraces);
    ui->mainToolBar->addWidget(lblBadgeDt);
    ui->mainToolBar->addWidget(lblBadgeTime);

    // 初始化为未载入状态
    updateDataBadges(QStringLiteral("未载入数据"), 0, 0, 0.0f);
}

void Pro_Seis_MASW::initUI()
{
    mainTabWidget = new QTabWidget(this);
    setCentralWidget(mainTabWidget);

    initMainTabs();
}

void Pro_Seis_MASW::initMainTabs()
{
    // =========================================================
    // --- 页面 1: 原始道集容器 (现代化卡片引导 + 拖拽支持) ---
    // =========================================================
    seismicViewContainer = new QWidget(this);
    seismicViewContainer->setAcceptDrops(true); // 开启拖拽支持

    QVBoxLayout* contLayout = new QVBoxLayout(seismicViewContainer);
    contLayout->setContentsMargins(0, 0, 0, 0);

    // 构建居中卡片容器
    QWidget* emptyCard = new QWidget(seismicViewContainer);
    QVBoxLayout* cardLayout = new QVBoxLayout(emptyCard);
    cardLayout->setAlignment(Qt::AlignCenter);
    cardLayout->setSpacing(16);

    // 1. 图标与大标题
    QLabel* lblIcon = new QLabel(QStringLiteral("🌊"), emptyCard);
    lblIcon->setStyleSheet("font-size: 56px; border: none;");
    lblIcon->setAlignment(Qt::AlignCenter);

    QLabel* lblTitle = new QLabel(QStringLiteral("暂未载入地震道集数据"), emptyCard);
    lblTitle->setStyleSheet("color: #f1f5f9; font-size: 20px; font-weight: bold; border: none;");
    lblTitle->setAlignment(Qt::AlignCenter);

    QLabel* lblSub = new QLabel(QStringLiteral("支持标准 SEG-Y / SGY 二维主动源面波道集 (IEEE / IBM 浮点格式)\n可点击下方按钮选择文件，或直接将 .sgy 文件拖拽至此窗口"), emptyCard);
    lblSub->setStyleSheet("color: #94a3b8; font-size: 13px; line-height: 1.5; border: none;");
    lblSub->setAlignment(Qt::AlignCenter);

    // 2. 主操作大按钮
    QPushButton* btnBigOpen = new QPushButton(QStringLiteral("📂 浏览并打开 SEGY 数据"), emptyCard);
    btnBigOpen->setCursor(Qt::PointingHandCursor);
    btnBigOpen->setStyleSheet(
        "QPushButton {"
        "   background-color: #0284c7; color: white; font-weight: bold; font-size: 14px;"
        "   padding: 10px 28px; border-radius: 6px; border: 1px solid #0369a1;"
        "}"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(btnBigOpen, &QPushButton::clicked, this, &Pro_Seis_MASW::onOpenSegy);

    // 3. 快速试用小标签入口
    QWidget* demoWidget = new QWidget(emptyCard);
    QHBoxLayout* demoLayout = new QHBoxLayout(demoWidget);
    demoLayout->setSpacing(10);
    demoLayout->setAlignment(Qt::AlignCenter);

    QLabel* lblOr = new QLabel(QStringLiteral("或者快速体验："), demoWidget);
    lblOr->setStyleSheet("color: #64748b; font-size: 12px; border: none;");

    QPushButton* btnDemoSynthetic = new QPushButton(QStringLiteral("🧪 一键加载理论三层正演模型"), demoWidget);
    btnDemoSynthetic->setCursor(Qt::PointingHandCursor);
    btnDemoSynthetic->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #334155; padding: 4px 12px; border-radius: 4px; font-size: 12px; }"
        "QPushButton:hover { background-color: #334155; border-color: #38bdf8; }"
    );
    connect(btnDemoSynthetic, &QPushButton::clicked, this, &Pro_Seis_MASW::onSyntheticClicked);

    demoLayout->addWidget(lblOr);
    demoLayout->addWidget(btnDemoSynthetic);

    // 装配进卡片
    cardLayout->addStretch();
    cardLayout->addWidget(lblIcon);
    cardLayout->addWidget(lblTitle);
    cardLayout->addWidget(lblSub);
    cardLayout->addWidget(btnBigOpen, 0, Qt::AlignCenter);
    cardLayout->addWidget(demoWidget, 0, Qt::AlignCenter);
    cardLayout->addStretch();

    contLayout->addWidget(emptyCard);

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

        // =========================================================
        // --- 动作 2: 新增【平滑插值 / 清晰色块】切换开关 ---
        // =========================================================
        QAction* actInterpolate = menu.addAction(QStringLiteral("平滑插值 (Smooth Blur)"));
        actInterpolate->setCheckable(true);                  // 设为可勾选项
        actInterpolate->setChecked(m_interpolateColorMap);   // 保持与当前状态同步
        connect(actInterpolate, &QAction::toggled, this, [=](bool checked) {
            m_interpolateColorMap = checked;
            if (dispColorMap) {
                dispColorMap->setInterpolate(checked);       // true: 开启模糊平滑，false: 纯净色块
                plotDispersion->replot();
            }
            });

        menu.addSeparator();

        // --- 动作 3: 保存图片 ---
        QAction* actSave = menu.addAction(QStringLiteral("保存图片 (Export PNG)..."));
        connect(actSave, &QAction::triggered, this, [=]() {
            QString defaultName = QString("Dispersion_%1.png")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            QString fileName = QFileDialog::getSaveFileName(
                this, QStringLiteral("保存频散谱图像"), defaultName, QStringLiteral("PNG 图片 (*.png);;所有文件 (*.*)"));
            if (!fileName.isEmpty()) {
                plotDispersion->savePng(fileName, 0, 0, 2.0, 100);
            }
            });

        // --- 动作 4: 导出为 SEGY ---
        QAction* actExportSegy = menu.addAction(QStringLiteral("💾 导出频散能量谱为 SEGY (*.sgy)..."));
        connect(actExportSegy, &QAction::triggered, this, &Pro_Seis_MASW::exportDispersionToSegy);

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
        this, &Pro_Seis_MASW::renderDispersionMap);
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
    connect(comboDispCmap, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Pro_Seis_MASW::updateDispersionColormap);
    connect(chkDispInv, &QCheckBox::toggled, this, &Pro_Seis_MASW::updateDispersionColormap);

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

    // 找到 initMainTabs() 中配置 dispBottomBar 拾取按钮的地方，追加：

    btnAiPick = new QPushButton(QStringLiteral("🤖 AI 一键拾取"), dispBottomBar);
    btnAiPick->setStyleSheet(
        "QPushButton {"
        "   background-color: #7c3aed;"       // 亮紫色，突出 AI 科技感
        "   border: 1px solid #6d28d9;"
        "   color: white;"
        "   font-weight: bold;"
        "   border-radius: 4px;"
        "   padding: 4px 10px;"
        "   min-height: 26px;"
        "}"
        "QPushButton:hover { background-color: #6d28d9; }"
        "QPushButton:pressed { background-color: #5b21b6; }"
    );
    connect(btnAiPick, &QPushButton::clicked, this, &Pro_Seis_MASW::onAiPickClicked);

    // 将 AI 按钮放在“拾取模式”复选框的旁边
    barLayout->addWidget(vline);
    barLayout->addWidget(chkPickMode);
    barLayout->addWidget(btnAiPick);          // <--- 插入 AI 拾取按钮
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

    connect(btnExportCurve, &QPushButton::clicked, this, &Pro_Seis_MASW::exportPickedCurve);

    // 4. 监听热力图点击事件
    connect(plotDispersion, &QCustomPlot::mousePress, this, &Pro_Seis_MASW::onDispersionPlotClicked);

    // --- 页面 3: 提取的频散曲线对比 ---
    plotCurve1D = new Data_show::Plot1D(this);
    plotCurve1D->m_setName(QStringLiteral("频散曲线对比 (Dispersion Curves)"));

    // ---------------------------------------------------------
    // --- 页面 4: 1D 速度结构剖面展示 (Tab 4: Vs Profile) ---
    // ---------------------------------------------------------
    inversionContainer = new QWidget(this);
    QVBoxLayout* invMainLayout = new QVBoxLayout(inversionContainer);
    invMainLayout->setContentsMargins(4, 4, 4, 4);
    invMainLayout->setSpacing(4);

    // 1. 中间主要内容区：左边剖面图，右边地层表格
    QWidget* contentWidget = new QWidget(inversionContainer);
    QHBoxLayout* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    // 左侧：1D 速度阶梯图 (Plot1D)
    plotVsProfile = new Data_show::Plot1D(contentWidget);
    plotVsProfile->m_setName(QStringLiteral("一维横波速度结构剖面 (1D Vs Profile)"));
    plotVsProfile->xAxis->setLabel(QStringLiteral("横波速度 Shear-Wave Velocity Vs (m/s)"));
    plotVsProfile->yAxis->setLabel(QStringLiteral("地下深度 Depth (m)"));
    plotVsProfile->yAxis->setRangeReversed(true); // 深度向下递增 (地学标准)
    contentLayout->addWidget(plotVsProfile, 7);   // 权重 7 (占 70% 宽度)

    // 右侧：地层参数表格 (QTableWidget)
    // ---------------------------------------------------------
    // 优化后的右侧地层参数表格 (QTableWidget)
    // ---------------------------------------------------------
    tableVsModel = new QTableWidget(contentWidget);
    tableVsModel->setColumnCount(5);
    tableVsModel->setHorizontalHeaderLabels({
        QStringLiteral("层号"), QStringLiteral("厚度(m)"), QStringLiteral("顶深(m)"),
        QStringLiteral("Vs(m/s)"), QStringLiteral("Vp(m/s)")
        });

    // 1. 【消除横向滚动条】：让 5 个列均匀拉伸撑满整个表格宽度
    tableVsModel->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableVsModel->verticalHeader()->setVisible(false);
    tableVsModel->setEditTriggers(QAbstractItemView::NoEditTriggers); // 只读
    tableVsModel->setSelectionBehavior(QAbstractItemView::SelectRows); // 整行选中
    tableVsModel->setAlternatingRowColors(true); // 开启隔行换色

    // 2. 【核心修复 QSS】：显式指定奇偶行背景色与高对比文字颜色
    tableVsModel->setStyleSheet(
        "QTableWidget {"
        "   background-color: #0f172a;"             // 奇数行底色 (深黑蓝)
        "   alternate-background-color: #1e293b;"   // 偶数行底色 (科技灰蓝，解决白底问题！)
        "   color: #f8fafc;"                        // 单元格文字颜色 (纯亮白，字字清晰)
        "   gridline-color: #334155;"               // 网格线颜色
        "   border: 1px solid #334155;"
        "   font-size: 13px;"
        "}"
        "QTableWidget::item {"
        "   padding: 4px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #0284c7;"             // 鼠标选中整行时的高亮科技蓝
        "   color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "   background-color: #1e293b;"
        "   color: #38bdf8;"                        // 表头天蓝色标题
        "   font-weight: bold;"
        "   font-size: 13px;"
        "   border: 1px solid #334155;"
        "   height: 30px;"
        "}"
    );

    contentLayout->addWidget(tableVsModel, 3); // 权重 3

    invMainLayout->addWidget(contentWidget, 1);

    // 2. 底部控制栏
    QFrame* invBottomBar = new QFrame(inversionContainer);
    invBottomBar->setFixedHeight(42);
    invBottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QHBoxLayout* invBarLayout = new QHBoxLayout(invBottomBar);
    invBarLayout->setContentsMargins(15, 0, 15, 0);
    invBarLayout->setSpacing(12);

    lblVsSummary = new QLabel(QStringLiteral("暂未载入反演地层模型"), invBottomBar);
    lblVsSummary->setStyleSheet("color: #38bdf8; font-family: Consolas; font-size: 12px;");

    btnLoadVsModel = new QPushButton(QStringLiteral("📂 载入反演模型文件 (*.txt)"), invBottomBar);
    btnLoadVsModel->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 5px 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(btnLoadVsModel, &QPushButton::clicked, this, &Pro_Seis_MASW::onLoadInversionModel);

    invBarLayout->addWidget(lblVsSummary);
    invBarLayout->addStretch();
    invBarLayout->addWidget(btnLoadVsModel);

    invMainLayout->addWidget(invBottomBar, 0);

    // 将四个标签页统一加入主窗口
    mainTabWidget->addTab(seismicViewContainer, QStringLiteral("1. 原始道集 (Shot Gather)"));
    mainTabWidget->addTab(dispersionContainer, QStringLiteral("2. 频散能量谱 (Dispersion Map)"));
    mainTabWidget->addTab(plotCurve1D, QStringLiteral("3. 频散曲线 (Extracted Curves)"));
    mainTabWidget->addTab(inversionContainer, QStringLiteral("4. 速度结构 (Vs Profile)")); // <--- 新增 Tab 4
}

void Pro_Seis_MASW::initControlDock()
{
    controlDock = new QDockWidget(QStringLiteral("控制面板"), this);
    controlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // 创建左侧 Tab 容器组件
    QTabWidget* dockTabs = new QTabWidget(controlDock);
    dockTabs->setTabPosition(QTabWidget::North);

    // =========================================================================
    // 【页面 1】频散分析与实测处理 (Tab 1: Dispersion Analysis)
    // =========================================================================
    QWidget* pageDispersion = new QWidget(dockTabs);
    QVBoxLayout* dispLayout = new QVBoxLayout(pageDispersion);
    dispLayout->setContentsMargins(8, 8, 8, 8);
    dispLayout->setSpacing(8);

    // --- 组 1: 采集与观测系统几何参数 (Geometry) ---
    QGroupBox* geomGroup = new QGroupBox(QStringLiteral("观测系统参数 (Geometry)"), pageDispersion);
    QFormLayout* geomLayout = new QFormLayout(geomGroup);
    geomLayout->setSpacing(6);

    spinDt = new QDoubleSpinBox(geomGroup);
    spinDt->setRange(0.001, 100.0);
    spinDt->setValue(1.0);
    spinDt->setDecimals(3);
    spinDt->setSuffix(" ms");
    spinDt->setToolTip(QStringLiteral("时间采样率 dt，打开 SEGY 时会自动从道头读取"));

    spinDx = new QDoubleSpinBox(geomGroup);
    spinDx->setRange(0.01, 500.0);
    spinDx->setValue(1.0);
    spinDx->setDecimals(2);
    spinDx->setSuffix(" m");

    spinOffset0 = new QDoubleSpinBox(geomGroup);
    spinOffset0->setRange(0.0, 1000.0);
    spinOffset0->setValue(10.0);
    spinOffset0->setSuffix(" m");

    geomLayout->addRow(QStringLiteral("采样间隔 (dt):"), spinDt);
    geomLayout->addRow(QStringLiteral("道间距 (dx):"), spinDx);
    geomLayout->addRow(QStringLiteral("最小炮检距 (x0):"), spinOffset0);
    dispLayout->addWidget(geomGroup);

    // --- 组 2: 频散计算扫描网格 (Scan Range) ---
    QGroupBox* scanGroup = new QGroupBox(QStringLiteral("频散扫描范围 (Scan Range)"), pageDispersion);
    QFormLayout* scanLayout = new QFormLayout(scanGroup);
    scanLayout->setSpacing(6);

    comboMethod = new QComboBox(scanGroup);//MVDR
    // 修改 comboMethod 的选项列表：
    comboMethod->addItems({
        QStringLiteral("移相法 (Phase Shift)"),
        QStringLiteral("F-K 变换法"),
        QStringLiteral("Capon (高分辨率 MVDR)"),
        QStringLiteral("倾斜叠加法 (Slant-Stack / τ-p)") // <--- 新增第 4 种方法
        });

    comboGridQuality = new QComboBox(scanGroup);
    comboGridQuality->addItem(QStringLiteral("快速预览 (100 × 100 - 极速)"), QPoint(100, 100));
    comboGridQuality->addItem(QStringLiteral("标准质量 (180 × 250 - 推荐)"), QPoint(180, 250));
    comboGridQuality->addItem(QStringLiteral("高精度 (300 × 500 - 精细)"), QPoint(300, 500));
    comboGridQuality->setCurrentIndex(1); // 默认选择【标准质量】

    // A. 算法模式变动时，自动更新底部芯片
    connect(comboMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        QString methodName = comboMethod->currentText();
        // 简化名字展示
        if (methodName.contains("移相法")) methodName = "移相法";
        else if (methodName.contains("F-K")) methodName = "F-K变换";
        else if (methodName.contains("MVDR")) methodName = "Capon/MVDR";
        else if (methodName.contains("倾斜叠加")) methodName = "Slant-Stack";

        statusChipMethod->setText(QString("<font color='#38bdf8'>●</font> <b>算法</b>: %1").arg(methodName));
        });

    // B. 网格质量变动时，自动更新底部芯片
    connect(comboGridQuality, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=]() {
        QPoint pt = comboGridQuality->currentData().toPoint();
        statusChipGrid->setText(QString("<font color='#f59e0b'>●</font> <b>网格</b>: %1×%2").arg(pt.x()).arg(pt.y()));
        });

    spinFmin = new QDoubleSpinBox(scanGroup);
    spinFmin->setRange(0.1, 500.0);
    spinFmin->setValue(5.0);
    spinFmin->setSuffix(" Hz");

    spinFreqMax = new QDoubleSpinBox(scanGroup);
    spinFreqMax->setRange(1.0, 1000.0);
    spinFreqMax->setValue(30.0);
    spinFreqMax->setSuffix(" Hz");

    spinVmin = new QDoubleSpinBox(scanGroup);
    spinVmin->setRange(10.0, 5000.0);
    spinVmin->setValue(200.0);
    spinVmin->setSuffix(" m/s");

    spinVmax = new QDoubleSpinBox(scanGroup);
    spinVmax->setRange(50.0, 8000.0);
    spinVmax->setValue(800.0);
    spinVmax->setSuffix(" m/s");

    scanLayout->addRow(QStringLiteral("计算方法:"), comboMethod);
    scanLayout->addRow(QStringLiteral("网格质量:"), comboGridQuality);
    scanLayout->addRow(QStringLiteral("频率下限 (Fmin):"), spinFmin);
    scanLayout->addRow(QStringLiteral("频率上限 (Fmax):"), spinFreqMax);
    scanLayout->addRow(QStringLiteral("速度下限 (Vmin):"), spinVmin);
    scanLayout->addRow(QStringLiteral("速度上限 (Vmax):"), spinVmax);
    dispLayout->addWidget(scanGroup);

    // --- 开始计算按钮 ---
    btnCalculate = new QPushButton(QStringLiteral("🚀 开始计算频散谱"), pageDispersion);
    btnCalculate->setObjectName("btnPrimary");
    btnCalculate->setMinimumHeight(38);
    connect(btnCalculate, &QPushButton::clicked, this, &Pro_Seis_MASW::onCalculateClicked);
    dispLayout->addWidget(btnCalculate);

    dispLayout->addStretch(); // 将控件往顶部压缩，防止变形

    // =========================================================================
    // 【页面 2】理论正演模拟 (Tab 2: Theoretical Synthesis)
    // =========================================================================
    QWidget* pageSynthetic = new QWidget(dockTabs);
    QVBoxLayout* synthLayout = new QVBoxLayout(pageSynthetic);
    synthLayout->setContentsMargins(8, 8, 8, 8);
    synthLayout->setSpacing(8);

    // --- 理论地质模型参数 (1D Model) ---
   // 找到 initControlDock() 中的【页面 2】理论正演模拟部分，替换 modelGroup：

    QGroupBox* modelGroup = new QGroupBox(QStringLiteral("三层地质模型参数 (3-Layer Model)"), pageSynthetic);
    QFormLayout* modelLayout = new QFormLayout(modelGroup);
    modelLayout->setSpacing(6);

    // --- 第 1 层 ---
    spinLayerH1 = new QDoubleSpinBox(modelGroup);
    spinLayerH1->setRange(0.5, 100.0);
    spinLayerH1->setValue(20.0); // 浅表层厚度 5m
    spinLayerH1->setSuffix(" m");

    spinLayerVs1 = new QDoubleSpinBox(modelGroup);
    spinLayerVs1->setRange(50.0, 2000.0);
    spinLayerVs1->setValue(300.0); // 表层横波 300 m/s
    spinLayerVs1->setSuffix(" m/s");

    // --- 第 2 层 ---
    spinLayerH2 = new QDoubleSpinBox(modelGroup);
    spinLayerH2->setRange(0.5, 200.0);
    spinLayerH2->setValue(100.0); // 中间层厚度 100m
    spinLayerH2->setSuffix(" m");

    spinLayerVs2 = new QDoubleSpinBox(modelGroup);
    spinLayerVs2->setRange(50.0, 3000.0);
    spinLayerVs2->setValue(600.0); // 中间层横波 600 m/s
    spinLayerVs2->setSuffix(" m/s");

    // --- 第 3 层 (基底) ---
    spinLayerVs3 = new QDoubleSpinBox(modelGroup);
    spinLayerVs3->setRange(100.0, 5000.0);
    spinLayerVs3->setValue(600.0); // 基底基岩横波 600 m/s
    spinLayerVs3->setSuffix(" m/s");

    // --- 子波主频 ---
    spinWaveletFm = new QDoubleSpinBox(modelGroup);
    spinWaveletFm->setRange(1.0, 200.0);
    spinWaveletFm->setValue(12.0); // 主频 12 Hz
    spinWaveletFm->setSuffix(" Hz");

    modelLayout->addRow(QStringLiteral("第 1 层厚度 (H1):"), spinLayerH1);
    modelLayout->addRow(QStringLiteral("第 1 层横波 (Vs1):"), spinLayerVs1);
    modelLayout->addRow(QStringLiteral("第 2 层厚度 (H2):"), spinLayerH2);
    modelLayout->addRow(QStringLiteral("第 2 层横波 (Vs2):"), spinLayerVs2);
    modelLayout->addRow(QStringLiteral("第 3 层基底 (Vs3):"), spinLayerVs3);
    modelLayout->addRow(QStringLiteral("雷克子波主频:"), spinWaveletFm);
    synthLayout->addWidget(modelGroup);

    // --- 一键合成理论面波记录按钮 ---
    QPushButton* btnSynthetic = new QPushButton(QStringLiteral("🧪 一键合成理论面波记录"), pageSynthetic);
    btnSynthetic->setStyleSheet(
        "QPushButton {"
        "   background-color: #059669;"
        "   border: 1px solid #047857;"
        "   color: white;"
        "   font-weight: bold;"
        "   border-radius: 4px;"
        "   padding: 6px 12px;"
        "   min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: #047857; }"
        "QPushButton:pressed { background-color: #065f46; }"
    );
    connect(btnSynthetic, &QPushButton::clicked, this, &Pro_Seis_MASW::onSyntheticClicked);
    synthLayout->addWidget(btnSynthetic);

    synthLayout->addStretch(); // 弹性占位

    // =========================================================================
    // 【页面 3】AI 训练集批量生成 (Tab 3: Dataset Generation)
    // =========================================================================
    QWidget* pageDatasetGen = new QWidget(dockTabs);
    QVBoxLayout* genLayout = new QVBoxLayout(pageDatasetGen);
    genLayout->setContentsMargins(8, 8, 8, 8);
    genLayout->setSpacing(10);

    QGroupBox* genGroup = new QGroupBox(QStringLiteral("批量生成配置"), pageDatasetGen);
    QFormLayout* formGen = new QFormLayout(genGroup);
    formGen->setSpacing(8);

    // 1. 样本数量设置
    spinSampleCount = new QSpinBox(genGroup);
    spinSampleCount->setRange(10, 50000);
    spinSampleCount->setValue(1000); // 默认批量生成 1000 个
    spinSampleCount->setSingleStep(100);
    spinSampleCount->setSuffix(QStringLiteral(" 组"));

    // 2. 输出路径设置
    QWidget* dirContainer = new QWidget(genGroup);
    QHBoxLayout* dirLayout = new QHBoxLayout(dirContainer);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(4);

    editOutputDir = new QLineEdit(dirContainer);
    // 默认指向之前创建的 AI 训练工作室目录
    editOutputDir->setText(QStringLiteral("D:/Code/visual_code/QT_project/Qt_Seis_MASW/AI_Train_MASW/dataset"));

    btnBrowseDir = new QPushButton(QStringLiteral("..."), dirContainer);
    btnBrowseDir->setFixedWidth(30);
    connect(btnBrowseDir, &QPushButton::clicked, this, [=]() {
        QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择数据集输出文件夹"), editOutputDir->text());
        if (!dir.isEmpty()) editOutputDir->setText(dir);
        });

    dirLayout->addWidget(editOutputDir);
    dirLayout->addWidget(btnBrowseDir);

    formGen->addRow(QStringLiteral("样本总数:"), spinSampleCount);
    formGen->addRow(QStringLiteral("输出目录:"), dirContainer);
    genLayout->addWidget(genGroup);

    // 3. 说明与进度条
    QLabel* lblTip = new QLabel(QStringLiteral("提示：将随机三层地质模型 (Vs: 150~1000 m/s)，正演并计算相移频散谱，写入纯二进制大文件。"), pageDatasetGen);
    lblTip->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lblTip->setWordWrap(true);
    genLayout->addWidget(lblTip);

    progressGen = new QProgressBar(pageDatasetGen);
    progressGen->setRange(0, 100);
    progressGen->setValue(0);
    progressGen->setTextVisible(true);
    progressGen->setStyleSheet(
        "QProgressBar { border: 1px solid #334155; border-radius: 4px; text-align: center; background: #0f172a; color: white; }"
        "QProgressBar::chunk { background-color: #0284c7; border-radius: 3px; }"
    );
    genLayout->addWidget(progressGen);

    // 4. 启动生成按钮
    btnStartGen = new QPushButton(QStringLiteral("🚀 批量生成训练集 (.bin)"), pageDatasetGen);
    btnStartGen->setStyleSheet(
        "QPushButton {"
        "   background-color: #7c3aed; border: 1px solid #6d28d9; color: white;"
        "   font-weight: bold; border-radius: 4px; padding: 8px; min-height: 32px;"
        "}"
        "QPushButton:hover { background-color: #6d28d9; }"
        "QPushButton:disabled { background-color: #475569; }"
    );
    connect(btnStartGen, &QPushButton::clicked, this, &Pro_Seis_MASW::onStartDatasetGeneration);
    genLayout->addWidget(btnStartGen);

    genLayout->addStretch();


    // =========================================================================
    // 装配 Tab 页面到 DockWidget
    // =========================================================================
    dockTabs->addTab(pageDispersion, QStringLiteral("📊 频散分析"));
    dockTabs->addTab(pageSynthetic, QStringLiteral("🧪 理论正演"));
    dockTabs->addTab(pageDatasetGen, QStringLiteral("📦 样本生成")); // <--- 新增 Tab 3

    controlDock->setWidget(dockTabs);
    addDockWidget(Qt::LeftDockWidgetArea, controlDock);
}

void Pro_Seis_MASW::initLogDock()
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
// ---------------------------------------------------------
// 核心：统一的 SEGY 解析与载入函数 (拖拽和按钮共用)
// ---------------------------------------------------------
void Pro_Seis_MASW::loadSegyFile(const QString& filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) return;

    QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    textLog->append(QStringLiteral("[%1] 正在载入数据: %2").arg(timeStr).arg(filePath));

    // 1. 读取数据
    m_seismicData = SeismicIO::readSegyFile2D(filePath.toStdString());


    if (m_seismicData.empty() || m_seismicData[0].empty()) {
        QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("SEGY 读取失败或数据为空！"));
        return;
    }

    int traces = m_seismicData.size();
    int samples = m_seismicData[0].size();
    float dt = static_cast<float>(spinDt->value() / 1000.0);

    textLog->append(QStringLiteral("[%1] 读取成功! 总道数: %2, 每道采样点: %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
        .arg(traces)
        .arg(samples));

    // 实时点亮顶部数据看板
    updateDataBadges(QFileInfo(filePath).fileName(), traces, samples, dt * 1000.0f);
    statusChipData->setText(QString("<font color='#10b981'>●</font> <b>数据</b>: %1道×%2点").arg(traces).arg(samples));

    // 2. 调用 SeismicView2D 模块生成视图组件并嵌入 Tab 1
    QWidget* seismicPlotWidget = createSeismicView(m_seismicData, QStringLiteral("道集剖面"), seismicViewContainer, dt);

    // 清空页面 1 原有内容（卡片）并填入新视图
    QLayout* layout = seismicViewContainer->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    layout->addWidget(seismicPlotWidget);

    // 自动切到第 1 页
    mainTabWidget->setCurrentIndex(0);
}

// ---------------------------------------------------------
// 按钮点击：只需弹窗选路径，然后交给 loadSegyFile 执行
// ---------------------------------------------------------
void Pro_Seis_MASW::onOpenSegy()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 SEGY 文件"),
        QString(),
        QStringLiteral("SEGY 地震数据 (*.sgy *.segy *.dat);;所有文件 (*.*)")
    );

    if (!fileName.isEmpty()) {
        loadSegyFile(fileName);
    }
}

void Pro_Seis_MASW::onCalculateClicked()
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


    int methodIndex = comboMethod->currentIndex(); // 0: 移相法, 1: F-K 变换法 2:MVDR


    if (methodIndex == 0) {
        textLog->append(QStringLiteral("[%1] 采用【移相法 (Phase Shift)】计算频散谱...")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        m_rawDispersionEnergy = computePhaseShiftDispersion(
            m_seismicData, dt, dx, x0, m_dispFmin, m_dispFmax, m_dispNf, m_dispVmin, m_dispVmax, m_dispNv);
    }
    else if (methodIndex == 1) {
        textLog->append(QStringLiteral("[%1] 采用【F-K 变换法】计算频散谱...")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        m_rawDispersionEnergy = computeFKDispersion(
            m_seismicData, dt, dx, m_dispFmin, m_dispFmax, m_dispNf, m_dispVmin, m_dispVmax, m_dispNv);
    }
    else if (methodIndex == 2) {
        textLog->append(QStringLiteral("[%1] 采用【Capon 高分辨率 (MVDR)】计算频散谱...")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        m_rawDispersionEnergy = computeCaponMVDRDispersion(
            m_seismicData, dt, dx, m_dispFmin, m_dispFmax, m_dispNf, m_dispVmin, m_dispVmax, m_dispNv);
    }
    else {
        textLog->append(QStringLiteral("[%1] 采用【倾斜叠加法 (Slant-Stack / τ-p)】计算频散谱...")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        m_rawDispersionEnergy = computeSlantStackDispersion(
            m_seismicData, dt, dx, x0, m_dispFmin, m_dispFmax, m_dispNf, m_dispVmin, m_dispVmax, m_dispNv);
    }
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

void Pro_Seis_MASW::updateDispersionColormap()
{
    if (!dispColorMap) return;

    QString cmapName = comboDispCmap->currentText();
    bool inv = chkDispInv->isChecked();

    QCPColorGradient grad = getScientificGradient(cmapName, inv);

    dispColorMap->setGradient(grad);
    plotDispersion->replot();
}

void Pro_Seis_MASW::renderDispersionMap()
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
    dispColorMap->setInterpolate(m_interpolateColorMap);

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

void Pro_Seis_MASW::onDispersionPlotClicked(QMouseEvent* event)
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

void Pro_Seis_MASW::updatePickVisuals()
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

void Pro_Seis_MASW::exportPickedCurve()
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

void Pro_Seis_MASW::exportDispersionToSegy()
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

void Pro_Seis_MASW::onSyntheticClicked()
{
    // 1. 动态从面板读取三层地质模型参数
    double h1 = spinLayerH1->value();
    double vs1 = spinLayerVs1->value();
    double h2 = spinLayerH2->value();
    double vs2 = spinLayerVs2->value();
    double vs3 = spinLayerVs3->value();
    double fm = spinWaveletFm->value();

    // 2. 组装三层介质物理模型
    LayerModel model;
    model.H = { h1, h2 };                     // 2 个层厚参数 (最后一层为无限半空间)
    model.VS = { vs1, vs2, vs3 };              // 3 层横波速度
    model.VP = { vs1 * 2.0, vs2 * 2.0, vs3 * 2.0 }; // 标称泊松介质 Vp = 2*Vs
    model.Rho = { 1800.0, 2000.0, 2200.0 };     // 各层密度 (递增)

    // 3. 读取几何与频率范围
    double dt = spinDt->value() / 1000.0;
    double dx = spinDx->value();
    double x0 = spinOffset0->value();
    double fmin = spinFmin->value();
    double fmax = spinFreqMax->value();

    // 采样频率序列 (步长 0.25 Hz，更密集的频点可以让三层频散拐折更平滑)
    std::vector<double> freqs;
    for (double f = fmin; f <= fmax; f += 0.25) freqs.push_back(f);

    textLog->append(QStringLiteral("[%1] 正在进行三层介质面波理论正演求解...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    // 4. 计算理论相速度曲线 (自动处理三层模型)
    auto theoVel = RayleighForwardSolver::calcBaseDispersion(freqs, model);

    // 5. 合成 96 道面波记录 (物理相移调制 + IFFT)
    int nTraces = 96;
    std::vector<double> offsets;
    for (int i = 0; i < nTraces; ++i) offsets.push_back(x0 + i * dx);

    m_seismicData = RayleighForwardSolver::synthesizeSurfaceWaveGather(
        freqs, theoVel, dt, 1000 /*nt=1000点*/, offsets, fm);

    // 6. 载入道集视图 (Tab 1)
    QWidget* plotWidget = createSeismicView(
        m_seismicData,
        QStringLiteral("三层介质理论面波剖面"),
        seismicViewContainer,
        static_cast<float>(dt) // <--- 传入当前正演设定的真实 dt
    );
    QLayout* layout = seismicViewContainer->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    layout->addWidget(plotWidget);
    mainTabWidget->setCurrentIndex(0);

    // 7. 将理论曲线更新至 Tab 3 (Plot1D)
    if (plotCurve1D) {
        plotCurve1D->setData(freqs, theoVel, QStringLiteral("三层介质理论基阶频散曲线 (3-Layer Theoretical)"));
        plotCurve1D->graph(0)->setPen(QPen(Qt::red, 2.0));
    }

    textLog->append(QStringLiteral("[%1] 三层介质面波正演完成: H=[%2, %3]m, Vs=[%4, %5, %6]m/s, 主频=%7Hz")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(h1).arg(h2)
        .arg(vs1).arg(vs2).arg(vs3)
        .arg(fm));

    // 理论数据点亮看板
    updateDataBadges(QStringLiteral("三层介质理论正演模型"), nTraces, 1000, dt * 1000.0f);
    statusChipData->setText(QStringLiteral("<font color='#10b981'>●</font> <b>数据</b>: 96道×1000点 (正演)"));
}

void Pro_Seis_MASW::onStartDatasetGeneration()
{
    QString outDir = editOutputDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先指定输出目录！"));
        return;
    }

    QDir dir(outDir);
    if (!dir.exists()) dir.mkpath(".");

    int totalSamples = spinSampleCount->value();
    btnStartGen->setEnabled(false);
    progressGen->setValue(0);

    // 提取当前的扫描网格参数 (保证与主程序完全一致)
    QPoint gridRes = comboGridQuality->currentData().toPoint();
    int nf = gridRes.x();
    int nv = gridRes.y();
    float fmin = static_cast<float>(spinFmin->value());
    float fmax = static_cast<float>(spinFreqMax->value());
    float vmin = static_cast<float>(spinVmin->value());
    float vmax = static_cast<float>(spinVmax->value());
    float dt = static_cast<float>(spinDt->value() / 1000.0);
    float dx = static_cast<float>(spinDx->value());
    float x0 = static_cast<float>(spinOffset0->value());

    textLog->append(QStringLiteral("[%1] 启动训练集生成，目标样本数: %2 组...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(totalSamples));

    // 使用 QThread 在子线程执行，防止主界面卡顿
    QThread* workerThread = QThread::create([=]() {
        QString inputBinPath = outDir + "/dataset_inputs.bin";
        QString labelBinPath = outDir + "/dataset_labels.bin";
        QString metaPath = outDir + "/dataset_meta.txt";

        std::ofstream fileInputs(inputBinPath.toStdString(), std::ios::binary | std::ios::trunc);
        std::ofstream fileLabels(labelBinPath.toStdString(), std::ios::binary | std::ios::trunc);

        // 随机参数生成器 (随机地质模型)
        std::mt19937 rng(1337); // 固定随机种子保证可复现
        std::uniform_real_distribution<double> distH1(2.0, 15.0);    // 第1层厚度: 2~15m
        std::uniform_real_distribution<double> distH2(5.0, 30.0);    // 第2层厚度: 5~30m
        std::uniform_real_distribution<double> distVs1(150.0, 320.0); // 浅层速度: 150~320 m/s
        std::uniform_real_distribution<double> distDVs2(50.0, 300.0); // 中层速度增量
        std::uniform_real_distribution<double> distDVs3(100.0, 500.0);// 基底层速度增量
        std::uniform_real_distribution<double> distFm(8.0, 20.0);     // 子波主频

        // 构建频率采样序列
        std::vector<double> freqs(nf);
        float df_scan = (fmax - fmin) / std::max(1, nf - 1);
        for (int i = 0; i < nf; ++i) freqs[i] = fmin + i * df_scan;

        // 检波器物理偏移距
        int nTraces = 96;
        std::vector<double> offsets(nTraces);
        for (int i = 0; i < nTraces; ++i) offsets[i] = x0 + i * dx;

        for (int s = 0; s < totalSamples; ++s) {
            // 1. 随机生成 3 层地质模型
            double h1 = distH1(rng);
            double h2 = distH2(rng);
            double vs1 = distVs1(rng);
            double vs2 = vs1 + distDVs2(rng);
            double vs3 = vs2 + distDVs3(rng);
            double fm = distFm(rng);

            LayerModel model;
            model.H = { h1, h2 };
            model.VS = { vs1, vs2, vs3 };
            model.VP = { vs1 * 2.0, vs2 * 2.0, vs3 * 2.0 };
            model.Rho = { 1800.0, 2000.0, 2200.0 };

            // 2. 正演理论基阶相速度
            auto theoVel = RayleighForwardSolver::calcBaseDispersion(freqs, model);

            // 3. 合成面波时域记录
            auto gather = RayleighForwardSolver::synthesizeSurfaceWaveGather(
                freqs, theoVel, dt, 1000, offsets, fm);

            // 4. 移相法计算频散能量谱
            auto energy = computePhaseShiftDispersion(
                gather, dt, dx, x0, fmin, fmax, nf, vmin, vmax, nv);

            // 5. 按频率列归一化
            for (int fi = 0; fi < nf; ++fi) {
                float colMax = 0.0f;
                for (int vi = 0; vi < nv; ++vi) colMax = std::max(colMax, energy[vi][fi]);
                if (colMax > 1e-6f) {
                    for (int vi = 0; vi < nv; ++vi) energy[vi][fi] /= colMax;
                }
            }

            // 6. 写入文件: inputs 写入 Nv * Nf 个 float32
            for (int vi = 0; vi < nv; ++vi) {
                fileInputs.write(reinterpret_cast<const char*>(energy[vi].data()), nf * sizeof(float));
            }

            // labels 写入 Nf 个 float32 (理论相速度真值)
            std::vector<float> labelFloat(nf);
            for (int fi = 0; fi < nf; ++fi) labelFloat[fi] = static_cast<float>(theoVel[fi]);
            fileLabels.write(reinterpret_cast<const char*>(labelFloat.data()), nf * sizeof(float));

            // 更新进度条 (每 10 个样本更新一次)
            if (s % 10 == 0 || s == totalSamples - 1) {
                int percent = (s + 1) * 100 / totalSamples;
                QMetaObject::invokeMethod(progressGen, "setValue", Qt::QueuedConnection, Q_ARG(int, percent));
            }
        }

        fileInputs.close();
        fileLabels.close();

        // 写入元数据 txt
        std::ofstream fileMeta(metaPath.toStdString());
        fileMeta << "samples=" << totalSamples << "\n"
            << "nv=" << nv << "\n"
            << "nf=" << nf << "\n"
            << "fmin=" << fmin << "\n"
            << "fmax=" << fmax << "\n"
            << "vmin=" << vmin << "\n"
            << "vmax=" << vmax << "\n";
        fileMeta.close();

        // 完成通知
        QMetaObject::invokeMethod(this, [=]() {
            btnStartGen->setEnabled(true);
            textLog->append(QStringLiteral("[%1] 训练集批量生成完毕！保存于: %2")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(outDir));
            QMessageBox::information(this, QStringLiteral("生成完成"),
                QStringLiteral("已成功生成 %1 组训练样本！\n\n• 输入特征: dataset_inputs.bin\n• 理论真值: dataset_labels.bin\n• 元数据: dataset_meta.txt")
                .arg(totalSamples));
            }, Qt::QueuedConnection);
        });

    workerThread->start();
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
}

void Pro_Seis_MASW::onAiPickClicked()
{
    // 1. 基础有效性检查
    if (m_rawDispersionEnergy.empty() || m_dispNf <= 0 || m_dispNv <= 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先计算或载入频散能量谱！"));
        return;
    }

    // 自动寻找模型路径 (支持开发环境与最终打包环境)
    QString modelPath = QCoreApplication::applicationDirPath() + "/models/dispersion_picker.onnx";
    if (!QFile::exists(modelPath)) {
        // 开发备选路径
        modelPath = QCoreApplication::applicationDirPath() + "/../../Pro_Seis_MASW/models/dispersion_picker.onnx";
    }
    if (!QFile::exists(modelPath)) {
        modelPath = QFileDialog::getOpenFileName(this, QStringLiteral("定位 AI 模型文件"), "", "ONNX Model (*.onnx)");
        if (modelPath.isEmpty()) return;
    }

    textLog->append(QStringLiteral("[%1] 启动 AI 智能脊线推理...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    try {
        // =========================================================
        // 2. 初始化 ONNX Runtime 推理会话 (纯 CPU 极速推理)
        // =========================================================
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "MASW_AI_Picker");
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(4); // 开启 4 线程加速
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Windows 下使用宽字符路径加载
        std::wstring wModelPath = modelPath.toStdWString();
        Ort::Session session(env, wModelPath.c_str(), sessionOptions);

        // =========================================================
        // 3. 构建模型输入张量 [1, 1, Nv, Nf]
        // =========================================================
        int nv = m_dispNv; // 250
        int nf = m_dispNf; // 180

        // 提取当前界面归一化后的数据并展平为一维数组 (行优先)
        std::vector<float> inputTensorValues;
        inputTensorValues.reserve(nv * nf);

        // 按照 Python 端一样的每列归一化标准送入
        for (int vi = 0; vi < nv; ++vi) {
            for (int fi = 0; fi < nf; ++fi) {
                inputTensorValues.push_back(m_rawDispersionEnergy[vi][fi]);
            }
        }

        std::vector<int64_t> inputDims = { 1, 1, nv, nf };
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
            inputDims.data(), inputDims.size()
        );

        const char* inputNames[] = { "input" };
        const char* outputNames[] = { "output" };

        // =========================================================
        // 4. 执行前向推理
        // =========================================================
        QElapsedTimer timer;
        timer.start();

        auto outputTensors = session.Run(
            Ort::RunOptions{ nullptr }, inputNames, &inputTensor, 1, outputNames, 1
        );

        qint64 elapsedMs = timer.elapsed();

        // 获取输出指针 (概率矩阵: 形状同为 [nv, nf])
        float* probMatrix = outputTensors.front().GetTensorMutableData<float>();

        // =========================================================
        // 5. 后处理：加权质心法提取亚像素 1D 频散曲线坐标
        // =========================================================
        m_pickedPoints.clear();
        double df = (m_dispFmax - m_dispFmin) / std::max(1, nf - 1);
        double dv = (m_dispVmax - m_dispVmin) / std::max(1, nv - 1);

        for (int fi = 0; fi < nf; ++fi) {
            // 找到当前列概率最大的点
            int maxVi = 0;
            float maxProb = -1.0f;
            for (int vi = 0; vi < nv; ++vi) {
                float p = probMatrix[vi * nf + fi];
                if (p > maxProb) {
                    maxProb = p;
                    maxVi = vi;
                }
            }

            // 局部质心平滑 (取峰值上下各 3 个像素)
            int viStart = std::max(0, maxVi - 3);
            int viEnd = std::min(nv - 1, maxVi + 3);
            double weightSum = 0.0;
            double idxWeightedSum = 0.0;

            for (int vi = viStart; vi <= viEnd; ++vi) {
                double w = probMatrix[vi * nf + fi];
                weightSum += w;
                idxWeightedSum += vi * w;
            }

            double subPixelVi = (weightSum > 1e-4) ? (idxWeightedSum / weightSum) : maxVi;

            // 换算为物理坐标
            double freq = m_dispFmin + fi * df;
            double vel = m_dispVmin + subPixelVi * dv;

            m_pickedPoints.append(QPointF(freq, vel));
        }

        // =========================================================
        // 6. 实时同步更新两处视图
        // =========================================================
        updatePickVisuals();

        textLog->append(QStringLiteral("[%1] 🤖 AI 频散曲线自动识别完成！共拾取 %2 个频点，推理耗时: %3 ms。")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
            .arg(m_pickedPoints.size())
            .arg(elapsedMs));

        QMessageBox::information(this, QStringLiteral("AI 拾取完成"),
            QStringLiteral("神经网络已完成全频段面波脊线提取！\n\n- 拾取点数: %1 点\n- 模型耗时: %2 ms\n- 结果已同步至 Tab 2 与 Tab 3。")
            .arg(m_pickedPoints.size()).arg(elapsedMs));

    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, QStringLiteral("ONNX 推理异常"), QString::fromLocal8Bit(e.what()));
    }
}

void Pro_Seis_MASW::onLoadInversionModel()
{
    QString defaultPath = QCoreApplication::applicationDirPath() + "/../../Pro_Seis_MASW/pro_data/output_curve/Inverted_Vs_Model.txt";
    if (!QFile::exists(defaultPath)) {
        defaultPath = QCoreApplication::applicationDirPath();
    }

    QString fileName = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择反演地层模型文件"),
        defaultPath,
        QStringLiteral("地层模型文本 (*.txt);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    displayInversionModel(fileName);
}

void Pro_Seis_MASW::displayInversionModel(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法读取模型文件：\n") + file.errorString());
        return;
    }

    struct LayerInfo {
        int layer;
        double thk;
        double depthTop;
        double vs;
        double vp;
    };

    QVector<LayerInfo> layers;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;

        QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (tokens.size() >= 5) {
            LayerInfo info;
            info.layer = tokens[0].toInt();
            info.thk = tokens[1].toDouble();
            info.depthTop = tokens[2].toDouble();
            info.vs = tokens[3].toDouble();
            info.vp = tokens[4].toDouble();
            layers.append(info);
        }
    }
    file.close();

    if (layers.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("未在文件中解析出有效的地层数据！"));
        return;
    }

    // =========================================================
    // 1. 构建垂直阶梯剖面坐标 (X: Vs, Y: Depth)
    // =========================================================
    std::vector<double> x_vs;
    std::vector<double> y_depth;

    tableVsModel->setRowCount(layers.size());

    double maxDepth = 0.0;
    for (int i = 0; i < layers.size(); ++i) {
        const auto& l = layers[i];

        // 填充右侧表格
        tableVsModel->setItem(i, 0, new QTableWidgetItem(QString::number(l.layer)));
        tableVsModel->setItem(i, 1, new QTableWidgetItem(l.thk > 0 ? QString::number(l.thk, 'f', 1) : QStringLiteral("无限")));
        tableVsModel->setItem(i, 2, new QTableWidgetItem(QString::number(l.depthTop, 'f', 1)));
        tableVsModel->setItem(i, 3, new QTableWidgetItem(QString::number(l.vs, 'f', 1)));
        tableVsModel->setItem(i, 4, new QTableWidgetItem(QString::number(l.vp, 'f', 1)));

        // 表格文字居中
        for (int c = 0; c < 5; ++c) {
            tableVsModel->item(i, c)->setTextAlignment(Qt::AlignCenter);
        }

        // 计算该层顶底深度
        double zTop = l.depthTop;
        double zBottom = (l.thk > 0) ? (zTop + l.thk) : (zTop + 12.0); // 最后一层半空间向下延伸 12m

        maxDepth = std::max(maxDepth, zBottom);

        // 构造垂直台阶点对：(Vs, zTop) -> (Vs, zBottom)
        x_vs.push_back(l.vs);
        y_depth.push_back(zTop);

        x_vs.push_back(l.vs);
        y_depth.push_back(zBottom);
    }

    // =========================================================
    // 2. 绘制 1D 阶梯折线 (在 Plot1D 上)
    // =========================================================
    plotVsProfile->setData(x_vs, y_depth, QStringLiteral("反演横波速度剖面 (Inverted Vs)"));

    // 设置阶梯曲线样式：深蓝色粗线条
    // 隐藏折线上的圆圈散点，改为纯净的平滑阶梯线
    if (plotVsProfile->graphCount() > 0) {
        plotVsProfile->graph(0)->setName(QStringLiteral("反演地层模型 (Inverted)"));
        plotVsProfile->graph(0)->setScatterStyle(QCPScatterStyle::ssNone); // 去除折点圆圈
        QPen p(QColor(2, 132, 199), 2.5);
        plotVsProfile->graph(0)->setPen(p);
    }

    // 坐标轴范围自适应
    plotVsProfile->xAxis->setLabel(QStringLiteral("横波速度 Vs (m/s)"));
    plotVsProfile->yAxis->setLabel(QStringLiteral("地下深度 Depth (m)"));
    plotVsProfile->yAxis->setRangeReversed(true); // 保证深度向下为正

    double vsMin = *std::min_element(x_vs.begin(), x_vs.end());
    double vsMax = *std::max_element(x_vs.begin(), x_vs.end());
    plotVsProfile->xAxis->setRange(vsMin - 40.0, vsMax + 40.0);
    plotVsProfile->yAxis->setRange(0.0, maxDepth);

    plotVsProfile->replot();

    // 3. 更新底栏摘要
    lblVsSummary->setText(QStringLiteral("已成功载入: %1 层模型 | 探测最大深度: %2 m | 基底 Vs: %3 m/s")
        .arg(layers.size())
        .arg(maxDepth, 0, 'f', 1)
        .arg(layers.last().vs, 0, 'f', 1));

    textLog->append(QStringLiteral("[%1] 速度剖面展示就绪: 成功读取反演地质模型 %2，共 %3 层。")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(QFileInfo(filePath).fileName())
        .arg(layers.size()));

    // 自动切换到 Tab 4
    mainTabWidget->setCurrentWidget(inversionContainer);
}

// ---------------------------------------------------------
// 刷新右侧数据看板胶囊标签
// ---------------------------------------------------------
void Pro_Seis_MASW::updateDataBadges(const QString& fileName, int traces, int samples, float dtMs)
{
    if (traces <= 0 || samples <= 0) {
        lblBadgeFile->setText(QStringLiteral("⚪ 状态: 等待载入数据"));
        lblBadgeTraces->setVisible(false);
        lblBadgeDt->setVisible(false);
        lblBadgeTime->setVisible(false);
        return;
    }

    lblBadgeTraces->setVisible(true);
    lblBadgeDt->setVisible(true);
    lblBadgeTime->setVisible(true);

    float totalTimeMs = (samples - 1) * dtMs;

    lblBadgeFile->setText(QString("📁 文件: <font color='#38bdf8'><b>%1</b></font>").arg(fileName));
    lblBadgeTraces->setText(QString("📊 道数: <font color='#38bdf8'><b>%1</b></font> 道").arg(traces));
    lblBadgeDt->setText(QString("⏱ dt: <font color='#38bdf8'><b>%1</b></font> ms").arg(dtMs, 0, 'f', 2));
    lblBadgeTime->setText(QString("⏳ 时长: <font color='#38bdf8'><b>%1</b></font> ms").arg(totalTimeMs, 0, 'f', 1));
}

// ---------------------------------------------------------
// 一键重置全部视图
// ---------------------------------------------------------
void Pro_Seis_MASW::onResetAll()
{
    // 复位频散谱视图
    if (plotDispersion) {
        if (m_dispFmax > m_dispFmin && m_dispVmax > m_dispVmin) {
            plotDispersion->xAxis->setRange(m_dispFmin, m_dispFmax);
            plotDispersion->yAxis->setRange(m_dispVmin, m_dispVmax);
        }
        else {
            plotDispersion->rescaleAxes();
        }
        plotDispersion->replot();
    }
    // 复位频散曲线视图
    if (plotCurve1D) {
        plotCurve1D->rescaleAxes();
        plotCurve1D->replot();
    }
    // 复位速度剖面视图
    if (plotVsProfile) {
        plotVsProfile->rescaleAxes();
        plotVsProfile->replot();
    }

    textLog->append(QStringLiteral("[%1] 🔄 所有图表视图已一键复位。")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

// ---------------------------------------------------------
// 帮助与说明弹窗
// ---------------------------------------------------------
void Pro_Seis_MASW::onShowHelp()
{
    QMessageBox::about(this, QStringLiteral("SeisTool-MASW 系统指南"),
        QStringLiteral("<h3>SeisTool-MASW 面波频散分析系统 v1.0</h3>"
            "<p>本系统用于近地表工程面波（MASW）高精度频散分析、理论正演与反演。</p>"
            "<b>核心工作流：</b>"
            "<ol>"
            "<li><b>Tab 1 原始道集</b>：拖拽或打开 SEGY 数据，Wiggle 与灰度剖面实时渲染；</li>"
            "<li><b>Tab 2 频散能量谱</b>：支持移相法、F-K法、高分辨率 MVDR、倾斜叠加法，点击【AI 智能拾取】一键成线；</li>"
            "<li><b>Tab 3 频散曲线</b>：期刊级曲线对比，支持导出反演所需 ASCII 数据；</li>"
            "<li><b>Tab 4 速度结构</b>：1D 横波速度 (Vs) 阶梯剖面展示与地层分层表格。</li>"
            "</ol>"
            "<b>操作小贴士：</b>"
            "<ul>"
            "<li>支持直接从桌面拖拽 .sgy 文件至窗口快速载入；</li>"
            "<li>频散谱右键支持重置、切换清晰色块 / 平滑插值，以及导出 SEGY；</li>"
            "<li>快捷键：<b>Ctrl+O</b> 打开数据 | <b>Ctrl+R</b> 开始计算。</li>"
            "</ul>"));
}

void Pro_Seis_MASW::initStatusBar()
{
    // 1. 状态栏底层深色仪器面板样式
    ui->statusBar->setFixedHeight(32);
    ui->statusBar->setStyleSheet(
        "QStatusBar {"
        "   background-color: #0b1120;"          // 深蓝黑底色 (终端感)
        "   color: #94a3b8;"                     // 默认提示文字为柔和灰
        "   border-top: 1px solid #1e293b;"      // 极细顶部分割线
        "   font-size: 12px;"
        "}"
        "QStatusBar::item {"
        "   border: none;"                       // 去除 Qt 默认的白色竖线分割
        "}"
    );

    // 2. 胶囊芯片通用微光样式
    QString chipStyle =
        "QLabel {"
        "   background-color: #0f172a;"
        "   border: 1px solid #334155;"
        "   border-radius: 11px;"
        "   padding: 2px 10px;"
        "   color: #cbd5e1;"
        "   font-family: 'Consolas', 'Segoe UI', monospace;"
        "   font-size: 11px;"
        "   margin-left: 6px;"
        "}";

    // 芯片 1: 算力与硬件状态
    statusChipHardware = new QLabel(this);
    statusChipHardware->setStyleSheet(chipStyle);
    statusChipHardware->setText(QStringLiteral("<font color='#10b981'>●</font> <b>引擎</b>: CPU多核/ONNX就绪"));

    // 芯片 2: 当前算法模式
    statusChipMethod = new QLabel(this);
    statusChipMethod->setStyleSheet(chipStyle);
    statusChipMethod->setText(QStringLiteral("<font color='#38bdf8'>●</font> <b>算法</b>: 移相法"));

    // 芯片 3: 当前计算网格
    statusChipGrid = new QLabel(this);
    statusChipGrid->setStyleSheet(chipStyle);
    statusChipGrid->setText(QStringLiteral("<font color='#f59e0b'>●</font> <b>网格</b>: 180×250"));

    // 芯片 4: 当前道集规格
    statusChipData = new QLabel(this);
    statusChipData->setStyleSheet(chipStyle);
    statusChipData->setText(QStringLiteral("<font color='#64748b'>○</font> <b>数据</b>: 未载入"));

    // 3. 通过 addPermanentWidget 将芯片永久锚定在状态栏右侧
    ui->statusBar->addPermanentWidget(statusChipData);
    ui->statusBar->addPermanentWidget(statusChipMethod); 
    ui->statusBar->addPermanentWidget(statusChipGrid);
    ui->statusBar->addPermanentWidget(statusChipHardware);

    ui->statusBar->showMessage(QStringLiteral("就绪 (Ready)"));
}

// 1. 鼠标拖拽进入窗口时触发：校验文件格式并放行
void Pro_Seis_MASW::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString file = urls.first().toLocalFile();
            if (file.endsWith(".sgy", Qt::CaseInsensitive) ||
                file.endsWith(".segy", Qt::CaseInsensitive) ||
                file.endsWith(".dat", Qt::CaseInsensitive))
            {
                event->acceptProposedAction(); // 接受拖入，光标变为可放置状态
                return;
            }
        }
    }
    event->ignore();
}

// 2. 【核心修复】：鼠标在窗口内晃动时必须持续接受，否则 Windows 会出现禁止圆圈 🚫
void Pro_Seis_MASW::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
    else {
        event->ignore();
    }
}

// 3. 鼠标松开释放时触发：提取路径并直接加载
void Pro_Seis_MASW::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            if (!filePath.isEmpty()) {
                event->acceptProposedAction();
                // 直接调用统一加载函数！
                loadSegyFile(filePath);
            }
        }
    }
}