#include "Pro_Seis_MASW.h"
#include "ui_Pro_Seis_MASW.h"
#include "qcustomplot.h"
#include "style.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <QGridLayout>
#include <QSpinBox>
#include <QSettings>
#include <QMenu>
#include <QRegularExpression>

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

static void applyApplicationTheme(QWidget* root, int themeIndex)
{
    using StyleHelper::ThemeMode;
    themeIndex = qBound(0, themeIndex, 9);
    root->setProperty("_currentThemeIndex", themeIndex);
    const ThemeMode mode = static_cast<ThemeMode>(themeIndex);
    const auto colors = StyleHelper::themeColors(mode);
    root->setStyleSheet(StyleHelper::getThemeStyle(mode));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(colors.bgApp));
    palette.setColor(QPalette::WindowText, QColor(colors.text));
    palette.setColor(QPalette::Base, QColor(colors.bgCard));
    palette.setColor(QPalette::AlternateBase, QColor(colors.bgHover));
    palette.setColor(QPalette::Text, QColor(colors.text));
    palette.setColor(QPalette::Button, QColor(colors.bgHeader));
    palette.setColor(QPalette::ButtonText, QColor(colors.text));
    palette.setColor(QPalette::Highlight, QColor(colors.primary));
    palette.setColor(QPalette::HighlightedText, QColor(colors.buttonText));
    QApplication::setPalette(palette);

    // Translate legacy per-widget dark QSS so existing controls follow the selected theme too.
    const QList<QWidget*> widgets = root->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget->property("_themeBaseStyleSaved").toBool()) {
            widget->setProperty("_themeBaseStyle", widget->styleSheet());
            widget->setProperty("_themeBaseStyleSaved", true);
        }
        QString qss = widget->property("_themeBaseStyle").toString();
        const QList<QPair<QString, QString>> replacements = {
            {"#0f172a", colors.bgApp}, {"#0b1120", colors.bgApp}, {"#0b1220", colors.bgApp},
            {"#111827", colors.bgHeader}, {"#1e293b", colors.bgCard}, {"#172554", colors.bgHover},
            {"#334155", colors.bgHeader}, {"#475569", colors.border}, {"#64748b", colors.textMuted},
            {"#94a3b8", colors.textSecondary}, {"#cbd5e1", colors.text}, {"#e2e8f0", colors.text},
            {"#f1f5f9", colors.text}, {"#f8fafc", colors.text}, {"#0284c7", colors.primary},
            {"#0369a1", colors.primary}, {"#38bdf8", colors.primary}
        };
        for (const auto& replacement : replacements)
            qss.replace(replacement.first, replacement.second, Qt::CaseInsensitive);
        widget->setStyleSheet(qss);
    }

    const QColor foreground(colors.text);
    const QColor background(colors.bgCard);
    const QColor grid("#cbd5df");
    for (QCustomPlot* plot : root->findChildren<QCustomPlot*>()) {
        plot->setBackground(QColor(colors.bgApp));
        plot->axisRect()->setBackground(background);
        const QList<QCPAxis*> axes = { plot->xAxis, plot->yAxis, plot->xAxis2, plot->yAxis2 };
        for (QCPAxis* axis : axes) {
            axis->setBasePen(QPen(foreground, 1));
            axis->setTickPen(QPen(foreground, 1));
            axis->setSubTickPen(QPen(foreground, 1));
            axis->setTickLabelColor(foreground);
            axis->setLabelColor(foreground);
            axis->grid()->setPen(QPen(grid, 1, Qt::DotLine));
        }
        if (auto* title = qobject_cast<QCPTextElement*>(plot->plotLayout()->element(0, 0)))
            title->setTextColor(foreground);
        plot->replot(QCustomPlot::rpQueuedReplot);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("Appearance/theme.v2"), themeIndex);
}

static void showThemeMenu(QWidget* parent, const QPoint& globalPos, int currentTheme, const std::function<void(int)>& onSelected)
{
    QMenu menu(parent);
    const QStringList names = { QStringLiteral("清爽米绿"), QStringLiteral("优雅薰衣草"), QStringLiteral("冰川静蓝"), QStringLiteral("夏日草甸"), QStringLiteral("经典白色 (MATLAB)"), QStringLiteral("深邃蓝灰"), QStringLiteral("海盐薄荷"), QStringLiteral("樱雾玫瑰"), QStringLiteral("暖阳砂岩"), QStringLiteral("暮色靛蓝") };
    for (int i = 0; i < names.size(); ++i) {
        QAction* action = menu.addAction(names[i]);
        action->setCheckable(true);
        action->setChecked(i == currentTheme);
        QObject::connect(action, &QAction::triggered, parent, [onSelected, i]() { onSelected(i); });
    }
    menu.exec(globalPos);
}

Pro_Seis_MASW::Pro_Seis_MASW(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::Pro_Seis_MASWClass)
{
    ui->setupUi(this);

    this->setWindowTitle(QStringLiteral("SeisTool-MASW 面波频散分析系统 v1.0"));
    this->setMinimumSize(1100,680);
    this->setAcceptDrops(true);
    setWindowIcon(QIcon(":/Pro_Seis_WASW/icon/layer.png"));
    resize(1366,800);

    createActionsAndToolBars(); // 建立工具栏和打开按钮
    initUI();
    initControlDock();
    initLogDock();

    initStatusBar(); // <--- 调用状态栏初始化

    const int savedTheme = QSettings().value(QStringLiteral("Appearance/theme.v2"), 4).toInt();
    applyApplicationTheme(this, savedTheme);

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

    QPushButton* btnTheme = new QPushButton(QStringLiteral("🎨"), this);
    btnTheme->setCursor(Qt::PointingHandCursor);
    btnTheme->setFixedSize(34, 30);
    btnTheme->setToolTip(QStringLiteral("界面风格：切换应用主题"));
    ui->mainToolBar->addWidget(btnTheme);
    connect(btnTheme, &QPushButton::clicked, this, [this, btnTheme]() {
        const int current = property("_currentThemeIndex").toInt();
        showThemeMenu(this, btnTheme->mapToGlobal(QPoint(0, btnTheme->height())), current,
            [this](int selected) { applyApplicationTheme(this, selected); });
    });

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
    dispBottomBar->setMinimumHeight(78);
    dispBottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QVBoxLayout* bottomBarLayout = new QVBoxLayout(dispBottomBar);
    bottomBarLayout->setContentsMargins(12, 5, 12, 5);
    bottomBarLayout->setSpacing(3);
    QHBoxLayout* displayControlsLayout = new QHBoxLayout();
    displayControlsLayout->setSpacing(7);
    QHBoxLayout* pickingControlsLayout = new QHBoxLayout();
    pickingControlsLayout->setSpacing(7);

    // 状态显示（鼠标当前坐标和能量值）
    lblDispStatus = new QLabel(QStringLiteral("就绪 (Ready)"), dispBottomBar);
    lblDispStatus->setStyleSheet("color: #38bdf8; font-family: Consolas; font-size: 12px; border: none;");

    // 1. 归一化模式下拉框
    QLabel* lblNorm = new QLabel(QStringLiteral("归一化:"), dispBottomBar);
    lblNorm->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    comboNormMode = new QComboBox(dispBottomBar);
    comboNormMode->setMinimumWidth(78);
    comboNormMode->setMaximumWidth(125);
    comboNormMode->addItem(QStringLiteral("逐频率")); // 索引 0: 默认推荐
    comboNormMode->addItem(QStringLiteral("全局"));   // 索引 1
    comboNormMode->setCurrentIndex(1);
    comboNormMode->setToolTip(QStringLiteral("逐频率归一化：逐频率列归一化；全局：保留不同频率间的能量差异。"));

    // 绑定信号：切换模式时瞬间重绘，无需重新计算算法！
    connect(comboNormMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &Pro_Seis_MASW::renderDispersionMap);
    // 色标切换控件
    QLabel* lblCmap = new QLabel(QStringLiteral("色带:"), dispBottomBar);
    lblCmap->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    comboDispCmap = new QComboBox(dispBottomBar);
    comboDispCmap->setMinimumWidth(70);
    comboDispCmap->setMaximumWidth(105);
    comboDispCmap->addItems({ "Jet", "Turbo", "Viridis", "Plasma", "Inferno", "Hot", "Seismic", "Grayscale" });

    chkDispInv = new QCheckBox(QStringLiteral("反转"), dispBottomBar);
    chkDispInv->setStyleSheet("color: #cbd5e1;");

    lblDispStatus->setMinimumWidth(120);
    lblDispStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    displayControlsLayout->addWidget(lblDispStatus, 1);
    displayControlsLayout->addWidget(lblNorm);
    displayControlsLayout->addWidget(comboNormMode);
    displayControlsLayout->addWidget(lblCmap);
    displayControlsLayout->addWidget(comboDispCmap);
    displayControlsLayout->addWidget(chkDispInv);
    bottomBarLayout->addLayout(displayControlsLayout);

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
    btnAiPick->setObjectName(QStringLiteral("btnPrimary"));
    btnAiPick->setMinimumHeight(26);
    connect(btnAiPick, &QPushButton::clicked, this, &Pro_Seis_MASW::onAiPickClicked);

    // 将 AI 按钮放在“拾取模式”复选框的旁边
    pickingControlsLayout->addWidget(chkPickMode);
    pickingControlsLayout->addWidget(btnAiPick);
    pickingControlsLayout->addWidget(btnUndoPick);
    pickingControlsLayout->addWidget(btnClearPick);
    pickingControlsLayout->addWidget(btnExportCurve);
    pickingControlsLayout->addStretch();
    bottomBarLayout->addLayout(pickingControlsLayout);

    dispMainLayout->addWidget(dispBottomBar, 0);

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
    //plotCurve1D = new Data_show::Plot1D(this);
    //plotCurve1D->m_setName(QStringLiteral("频散曲线对比 (Dispersion Curves)"));

    // =========================================================
    // --- 页面 3: 频散曲线多重对比 (Tab 3: Extracted Curves) ---
    // =========================================================
    curveCompareContainer = new QWidget(this);
    QVBoxLayout* curveMainLayout = new QVBoxLayout(curveCompareContainer);
    curveMainLayout->setContentsMargins(4, 4, 4, 4);
    curveMainLayout->setSpacing(4);

    // 1. 初始化 1D 图表画布
    plotCurve1D = new Data_show::Plot1D(curveCompareContainer);
    plotCurve1D->m_setName(QStringLiteral("多源频散曲线综合对比与质控"));
    plotCurve1D->xAxis->setLabel(QStringLiteral("频率 Frequency (Hz)"));
    plotCurve1D->yAxis->setLabel(QStringLiteral("相速度 Phase Velocity (m/s)"));

    // 2. 清空并预分配 3 个专属图层 (各司其职，互不干扰)
    plotCurve1D->clearGraphs();

    // 图层 1: 理论基阶曲线 (红实线)
    graphTheoretical = plotCurve1D->addGraph();
    graphTheoretical->setPen(QPen(Qt::red, 2.0));
    graphTheoretical->setName(QStringLiteral("理论基阶曲线 (Theoretical)"));

    // 图层 2: 实测拾取曲线 (深蓝线带红白圆点标记)
    graphPicked = plotCurve1D->addGraph();
    graphPicked->setPen(QPen(QColor(2, 132, 199), 1.8));
    graphPicked->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 6));
    graphPicked->setName(QStringLiteral("实测拾取点 (Observed/Picked)"));

    // 图层 3: 反演拟合曲线 (黑色加粗虚线)
    graphInverted = plotCurve1D->addGraph();
    graphInverted->setPen(QPen(Qt::black, 2.2, Qt::DashLine));
    graphInverted->setName(QStringLiteral("反演拟合曲线 (Inverted Fit)"));

    curveMainLayout->addWidget(plotCurve1D, 1);

    // 3. 底部曲线独立显隐控制栏 (Bottom Control Bar)
    QFrame* curveBottomBar = new QFrame(curveCompareContainer);
    curveBottomBar->setFixedHeight(42);
    curveBottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QHBoxLayout* barLayout2 = new QHBoxLayout(curveBottomBar);
    barLayout2->setContentsMargins(15, 0, 15, 0);
    barLayout2->setSpacing(16);

    QLabel* lblCtrlTitle = new QLabel(QStringLiteral("曲线显隐:"), curveBottomBar);
    lblCtrlTitle->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    chkShowTheoretical = new QCheckBox(QStringLiteral("理论基阶线 (红)"), curveBottomBar);
    chkShowTheoretical->setChecked(true);
    chkShowTheoretical->setStyleSheet("color: #f87171; font-weight: bold;"); // 柔和红

    chkShowPicked = new QCheckBox(QStringLiteral("实测拾取点 (蓝/点)"), curveBottomBar);
    chkShowPicked->setChecked(true);
    chkShowPicked->setStyleSheet("color: #38bdf8; font-weight: bold;"); // 天蓝

    chkShowInverted = new QCheckBox(QStringLiteral("反演拟合线 (黑虚)"), curveBottomBar);
    chkShowInverted->setChecked(true);
    chkShowInverted->setStyleSheet("color: #cbd5e1; font-weight: bold;");

    lblCurveMisfit = new QLabel(QStringLiteral("拟合残差 RMSE: 待计算"), curveBottomBar);
    lblCurveMisfit->setStyleSheet("color: #10b981; font-family: Consolas; font-size: 12px; border: none;");

    barLayout2->addWidget(lblCtrlTitle);
    barLayout2->addWidget(chkShowTheoretical);
    barLayout2->addWidget(chkShowPicked);
    barLayout2->addWidget(chkShowInverted);
    barLayout2->addStretch();
    barLayout2->addWidget(lblCurveMisfit);

    curveMainLayout->addWidget(curveBottomBar, 0);

    // 4. 绑定复选框与图层显隐联动
    connect(chkShowTheoretical, &QCheckBox::toggled, this, [=](bool on) {
        if (graphTheoretical) graphTheoretical->setVisible(on);
        plotCurve1D->replot();
        });
    connect(chkShowPicked, &QCheckBox::toggled, this, [=](bool on) {
        if (graphPicked) graphPicked->setVisible(on);
        plotCurve1D->replot();
        });
    connect(chkShowInverted, &QCheckBox::toggled, this, [=](bool on) {
        if (graphInverted) graphInverted->setVisible(on);
        plotCurve1D->replot();
        });

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
    // 找到 initMainTabs() 中配置 invBottomBar 的代码：
    QFrame* invBottomBar = new QFrame(inversionContainer);
    invBottomBar->setMinimumHeight(52);
    invBottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QHBoxLayout* invBarLayout = new QHBoxLayout(invBottomBar);
    invBarLayout->setContentsMargins(12, 6, 12, 6);
    invBarLayout->setSpacing(8);

    lblVsSummary = new QLabel(QStringLiteral("暂未载入反演地层模型"), invBottomBar);
    lblVsSummary->setStyleSheet("color: #38bdf8; font-family: Consolas; font-size: 12px; border: none;");
    lblVsSummary->setMinimumWidth(210);
    lblVsSummary->setToolTip(QStringLiteral("反演模型层数、有效深度与半空间横波速度"));

    // =========================================================
    // 【新增】：直接在底栏选择反演分层方案
    // =========================================================
    QLabel* lblInvLayers = new QLabel(QStringLiteral("反演分层:"), invBottomBar);
    lblInvLayers->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    comboInvLayers = new QComboBox(invBottomBar);
    comboInvLayers->setFixedWidth(200);
    comboInvLayers->setMinimumHeight(30);
    comboInvLayers->setToolTip(QStringLiteral("选择反演模型的层数和预设层厚。合成数据会根据正演模型自动选择两层或三层。"));
    comboInvLayers->setStyleSheet(
        "QComboBox { background-color: #1e293b; border: 1px solid #334155; color: #f8fafc; border-radius: 4px; padding: 3px 8px; }"
        "QComboBox QAbstractItemView { background-color: #1e293b; color: #f8fafc; selection-background-color: #0284c7; }"
    );
    comboInvLayers->addItem(QStringLiteral("两层模型 (单层覆盖+基底)"), 2);
    comboInvLayers->addItem(QStringLiteral("三层模型 (浅覆+过渡+基底)"), 3);
    comboInvLayers->addItem(QStringLiteral("四层模型 (浅层 0~16m)"), 4);
    comboInvLayers->addItem(QStringLiteral("六层模型 (0~30m 实测推荐)"), 6);
    comboInvLayers->addItem(QStringLiteral("八层模型 (0~45m 高密深部)"), 8);
    comboInvLayers->setCurrentIndex(0); // 默认选两层 (或实测时选6层)

    // 原有的反演按钮与载入按钮
    btnRunInversion = new QPushButton(QStringLiteral("🚀 开始 1D 速度反演"), invBottomBar);
    btnRunInversion->setMinimumHeight(32);
    btnRunInversion->setToolTip(QStringLiteral("使用 Tab 2 中已拾取的频散曲线反演 Vs 剖面；至少需要 4 个有效频点。"));
    btnRunInversion->setStyleSheet(
        "QPushButton {"
        "   background-color: #059669; color: white; font-weight: bold;"
        "   padding: 5px 14px; border-radius: 4px; min-height: 26px;"
        "}"
        "QPushButton:hover { background-color: #047857; }"
        "QPushButton:pressed { background-color: #065f46; }"
    );
    connect(btnRunInversion, &QPushButton::clicked, this, &Pro_Seis_MASW::onRunInversionClicked);

    btnLoadVsModel = new QPushButton(QStringLiteral("📂 载入模型 (*.txt)"), invBottomBar);
    btnLoadVsModel->setMinimumHeight(32);
    btnLoadVsModel->setToolTip(QStringLiteral("从文本文件载入已有的分层 Vs 模型"));
    btnLoadVsModel->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 5px 12px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(btnLoadVsModel, &QPushButton::clicked, this, &Pro_Seis_MASW::onLoadInversionModel);

    // 装配底栏：标签 -> 弹簧 -> 分层选择 -> 开始反演 -> 载入模型
    invBarLayout->addWidget(lblVsSummary);
    invBarLayout->addStretch();
    invBarLayout->addWidget(lblInvLayers);
    invBarLayout->addWidget(comboInvLayers);      // <--- 紧挨着反演按钮
    invBarLayout->addWidget(btnRunInversion);
    invBarLayout->addWidget(btnLoadVsModel);

    invMainLayout->addWidget(invBottomBar, 0);

    // ---------------------------------------------------------
    // --- 页面 5：由 1D Vs 模型生成的二维演示剖面 ---
    // ---------------------------------------------------------
    section2DContainer = new QWidget(this);
    section2DContainer->setObjectName(QStringLiteral("section2DContainer"));
    QVBoxLayout* sectionMainLayout = new QVBoxLayout(section2DContainer);
    sectionMainLayout->setContentsMargins(8, 8, 8, 8);
    sectionMainLayout->setSpacing(8);

    QLabel* sectionInfo = new QLabel(
        QStringLiteral("二维初始模型：将当前 1D Vs 分层结果沿测线方向扩展，并可加入平滑横向扰动。此页面用于界面与流程验证，不代表二维反演结果。"),
        section2DContainer);
    sectionInfo->setObjectName(QStringLiteral("sectionInfo"));
    sectionInfo->setWordWrap(true);
    sectionMainLayout->addWidget(sectionInfo);

    QFrame* sectionControlBar = new QFrame(section2DContainer);
    sectionControlBar->setStyleSheet("QFrame { background-color: transparent; border: none; }");
    QHBoxLayout* sectionControlLayout = new QHBoxLayout(sectionControlBar);
    sectionControlLayout->setContentsMargins(0, 0, 0, 0);
    sectionControlLayout->setSpacing(10);

    auto makeSectionGroup = [sectionControlBar](const QString& title) {
        QGroupBox* group = new QGroupBox(title, sectionControlBar);
        return group;
    };
    auto addSectionField = [](QHBoxLayout* layout, const QString& labelText, QWidget* field) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet(QStringLiteral("background: transparent; border: none; font-weight: 600;"));
        layout->addWidget(label);
        layout->addWidget(field);
    };

    QGroupBox* geometryGroup = makeSectionGroup(QStringLiteral("测线与网格"));
    QHBoxLayout* geometryLayout = new QHBoxLayout(geometryGroup);
    geometryLayout->setContentsMargins(4, 4, 4, 2);
    geometryLayout->setSpacing(8);

    spinSectionLength = new QDoubleSpinBox(geometryGroup);
    spinSectionLength->setRange(10.0, 5000.0);
    spinSectionLength->setValue(100.0);
    spinSectionLength->setDecimals(0);
    spinSectionLength->setSuffix(QStringLiteral(" m"));
    spinSectionLength->setFixedWidth(98);
    addSectionField(geometryLayout, QStringLiteral("长度"), spinSectionLength);

    spinSectionNx = new QSpinBox(geometryGroup);
    spinSectionNx->setRange(21, 501);
    spinSectionNx->setSingleStep(20);
    spinSectionNx->setValue(121);
    spinSectionNx->setFixedWidth(68);
    addSectionField(geometryLayout, QStringLiteral("横向"), spinSectionNx);

    spinSectionNz = new QSpinBox(geometryGroup);
    spinSectionNz->setRange(21, 501);
    spinSectionNz->setSingleStep(20);
    spinSectionNz->setValue(121);
    spinSectionNz->setFixedWidth(68);
    addSectionField(geometryLayout, QStringLiteral("深度"), spinSectionNz);
    sectionControlLayout->addWidget(geometryGroup);

    QGroupBox* displayGroup = makeSectionGroup(QStringLiteral("模拟与显示"));
    QHBoxLayout* displayLayout = new QHBoxLayout(displayGroup);
    displayLayout->setContentsMargins(4, 4, 4, 2);
    displayLayout->setSpacing(8);

    spinSectionVariation = new QDoubleSpinBox(displayGroup);
    spinSectionVariation->setRange(0.0, 25.0);
    spinSectionVariation->setValue(8.0);
    spinSectionVariation->setDecimals(0);
    spinSectionVariation->setSuffix(QStringLiteral(" %"));
    spinSectionVariation->setFixedWidth(72);
    spinSectionVariation->setToolTip(QStringLiteral("0% 为纯 1D 横向外推；增大后加入平滑、连续的合成横向速度变化。"));
    addSectionField(displayLayout, QStringLiteral("横向变化"), spinSectionVariation);

    comboSectionPalette = new QComboBox(displayGroup);
    comboSectionPalette->addItems({ QStringLiteral("Turbo"), QStringLiteral("Viridis"), QStringLiteral("Jet"), QStringLiteral("Inferno") });
    comboSectionPalette->setFixedWidth(92);
    addSectionField(displayLayout, QStringLiteral("色带"), comboSectionPalette);
    sectionControlLayout->addWidget(displayGroup);

    btnGenerateSection = new QPushButton(QStringLiteral("生成 / 刷新剖面"), sectionControlBar);
    btnGenerateSection->setObjectName(QStringLiteral("btnPrimary"));
    btnGenerateSection->setMinimumHeight(36);
    btnGenerateSection->setMinimumWidth(112);
    btnGenerateSection->setEnabled(false);
    sectionControlLayout->addWidget(btnGenerateSection);
    sectionControlLayout->addStretch(1);
    sectionMainLayout->addWidget(sectionControlBar);

    plotVsSection = new QCustomPlot(section2DContainer);
    plotVsSection->setBackground(Qt::white);
    plotVsSection->axisRect()->setBackground(Qt::white);
    plotVsSection->plotLayout()->insertRow(0);
    QCPTextElement* sectionTitle = new QCPTextElement(plotVsSection,
        QStringLiteral("二维横波速度剖面 (模拟初始模型)"), QFont("Microsoft YaHei", 12, QFont::Bold));
    sectionTitle->setTextColor(QColor("#0f172a"));
    plotVsSection->plotLayout()->addElement(0, 0, sectionTitle);
    plotVsSection->plotLayout()->insertColumn(1);
    vsSectionColorScale = new QCPColorScale(plotVsSection);
    vsSectionColorScale->setType(QCPAxis::atRight);
    vsSectionColorScale->axis()->setLabel(QStringLiteral("Vs (m/s)"));
    plotVsSection->plotLayout()->addElement(1, 1, vsSectionColorScale);
    plotVsSection->xAxis->setLabel(QStringLiteral("沿测线距离 Distance (m)"));
    plotVsSection->yAxis->setLabel(QStringLiteral("地下深度 Depth (m)"));
    plotVsSection->yAxis->setRangeReversed(true);
    const QColor axisColor("#111827");
    const QPen axisPen(QColor("#111827"), 1.0);
    for (QCPAxis* axis : { plotVsSection->xAxis, plotVsSection->yAxis,
                           plotVsSection->xAxis2, plotVsSection->yAxis2,
                           vsSectionColorScale->axis() }) {
        axis->setBasePen(axisPen);
        axis->setTickPen(axisPen);
        axis->setSubTickPen(axisPen);
        axis->setTickLabelColor(axisColor);
        axis->setLabelColor(axisColor);
        axis->setLabelFont(QFont("Segoe UI", 10, QFont::DemiBold));
        axis->setTickLabelFont(QFont("Segoe UI", 9));
        axis->grid()->setPen(QPen(QColor(203, 213, 225, 220), 1, Qt::DotLine));
    }
    plotVsSection->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    sectionMainLayout->addWidget(plotVsSection, 1);

    lblSectionStatus = new QLabel(QStringLiteral("请先在页面 4 完成 1D 反演或载入模型。"), section2DContainer);
    lblSectionStatus->setStyleSheet(QStringLiteral("background: transparent; padding: 2px 4px; border: none;"));
    sectionMainLayout->addWidget(lblSectionStatus);
    connect(btnGenerateSection, &QPushButton::clicked, this, &Pro_Seis_MASW::update2DVsSection);

    // 将五个标签页统一加入主窗口
    mainTabWidget->addTab(seismicViewContainer, QStringLiteral("1. 原始道集 (Shot Gather)"));
    mainTabWidget->addTab(dispersionContainer, QStringLiteral("2. 频散能量谱 (Dispersion Map)"));
    mainTabWidget->addTab(curveCompareContainer, QStringLiteral("3. 频散曲线 (Extracted Curves)")); // <--- 替换为容器
    mainTabWidget->addTab(inversionContainer, QStringLiteral("4. 速度结构 (Vs Profile)"));
    mainTabWidget->addTab(section2DContainer, QStringLiteral("5. 二维 Vs 剖面 (2D Section)"));
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

    // --- 1. 经典地学模型快速预设 ---
    QGroupBox* presetGroup = new QGroupBox(QStringLiteral("地学场景快速预设"), pageSynthetic);
    QVBoxLayout* presetLayout = new QVBoxLayout(presetGroup);
    presetLayout->setContentsMargins(6, 6, 6, 6);

    QComboBox* comboModelPreset = new QComboBox(presetGroup);
    // 【新增两层模型选项】
    comboModelPreset->addItem(QStringLiteral("经典两层模型 (单层覆盖层在基底上)")); // 索引 0: 经典双层
    comboModelPreset->addItem(QStringLiteral("标准三层递增型 (覆盖-风化-基岩)"));     // 索引 1: 经典三层
    comboModelPreset->addItem(QStringLiteral("低速夹层型 (软弱夹层/速度倒转)"));     // 索引 2: 速度倒转
    comboModelPreset->addItem(QStringLiteral("浅层坚硬基岩型 (极薄覆盖强波阻抗)")); // 索引 3: 浅基岩
    presetLayout->addWidget(comboModelPreset);
    synthLayout->addWidget(presetGroup);

    // --- 2. 地质模型参数控件 ---
    QGroupBox* modelGroup = new QGroupBox(QStringLiteral("地质分层模型参数 (1D Model)"), pageSynthetic);
    QFormLayout* modelLayout = new QFormLayout(modelGroup);
    modelLayout->setSpacing(5);

    // 第 1 层
    spinLayerH1 = new QDoubleSpinBox(modelGroup);
    spinLayerH1->setRange(0.5, 50.0);
    spinLayerH1->setValue(10.0); // 默认双层覆盖厚度 10 米
    spinLayerH1->setSuffix(" m");

    spinLayerVs1 = new QDoubleSpinBox(modelGroup);
    spinLayerVs1->setRange(50.0, 1500.0);
    spinLayerVs1->setValue(300.0); // 默认表层横波 300 m/s
    spinLayerVs1->setSuffix(" m/s");

    // 第 2 层
    spinLayerH2 = new QDoubleSpinBox(modelGroup);
    spinLayerH2->setRange(0.5, 80.0);
    spinLayerH2->setValue(10.0);
    spinLayerH2->setSuffix(" m");

    spinLayerVs2 = new QDoubleSpinBox(modelGroup);
    spinLayerVs2->setRange(50.0, 2500.0);
    spinLayerVs2->setValue(400.0);
    spinLayerVs2->setSuffix(" m/s");

    // 第 3 层 (基底半空间)
    spinLayerVs3 = new QDoubleSpinBox(modelGroup);
    spinLayerVs3->setRange(100.0, 5000.0);
    spinLayerVs3->setValue(600.0); // 默认基底横波 600 m/s
    spinLayerVs3->setSuffix(" m/s");

    // 子波主频
    spinWaveletFm = new QDoubleSpinBox(modelGroup);
    spinWaveletFm->setRange(2.0, 100.0);
    spinWaveletFm->setValue(12.0); // 默认 12 Hz
    spinWaveletFm->setSuffix(" Hz");

    modelLayout->addRow(QStringLiteral("第 1 层厚度 (H1):"), spinLayerH1);
    modelLayout->addRow(QStringLiteral("第 1 层横波 (Vs1):"), spinLayerVs1);
    modelLayout->addRow(QStringLiteral("第 2 层厚度 (H2):"), spinLayerH2);
    modelLayout->addRow(QStringLiteral("第 2 层横波 (Vs2):"), spinLayerVs2);
    modelLayout->addRow(QStringLiteral("基底横波 (Vs):"), spinLayerVs3);
    modelLayout->addRow(QStringLiteral("雷克子波主频:"), spinWaveletFm);
    synthLayout->addWidget(modelGroup);

    // --- 3. 操作按钮区 ---
    QPushButton* btnQuickCurve = new QPushButton(QStringLiteral("📈 仅预览理论频散线 (0毫秒)"), pageSynthetic);
    btnQuickCurve->setStyleSheet(
        "QPushButton { background-color: #1e293b; color: #38bdf8; border: 1px solid #334155; padding: 4px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #334155; border-color: #38bdf8; }"
    );
    synthLayout->addWidget(btnQuickCurve);

    QPushButton* btnSynthetic = new QPushButton(QStringLiteral("🧪 一键合成理论面波记录"), pageSynthetic);
    btnSynthetic->setObjectName(QStringLiteral("btnPrimary"));
    btnSynthetic->setMinimumHeight(36);
    connect(btnSynthetic, &QPushButton::clicked, this, &Pro_Seis_MASW::onSyntheticClicked);
    synthLayout->addWidget(btnSynthetic);

    synthLayout->addStretch();

    // =========================================================
    // 4. 预设联动：自动控制两层/三层控件的可用性
    // =========================================================
    auto updatePreset = [=](int idx) {
        if (idx == 0) {
            // 【两层经典模型】：第2层自动置灰禁用，第3层直接作为基底
            spinLayerH2->setEnabled(false);
            spinLayerVs2->setEnabled(false);
            spinLayerH1->setValue(10.0);   spinLayerVs1->setValue(300.0);
            spinLayerVs3->setValue(600.0); // 基底 Vs = 600 m/s
            spinWaveletFm->setValue(12.0);
        }
        else {
            // 三层模型：全部控件恢复可用
            spinLayerH2->setEnabled(true);
            spinLayerVs2->setEnabled(true);

            if (idx == 1) {
                // 标准三层递增型 (土 -> 砾石 -> 基岩)
                spinLayerH1->setValue(10.0);   spinLayerVs1->setValue(200.0);
                spinLayerH2->setValue(10.0);   spinLayerVs2->setValue(380.0);
                spinLayerVs3->setValue(750.0);
                spinWaveletFm->setValue(15.0);
            }
            else if (idx == 2) {
                // 低速夹层型 (硬壳层 -> 软弱层 -> 基底)
                spinLayerH1->setValue(10.0);   spinLayerVs1->setValue(350.0);
                spinLayerH2->setValue(10.0);   spinLayerVs2->setValue(160.0);
                spinLayerVs3->setValue(600.0);
                spinWaveletFm->setValue(12.0);
            }
            else if (idx == 3) {
                // 浅基岩型 (薄层覆盖 -> 坚硬基岩)
                spinLayerH1->setValue(10.0);   spinLayerVs1->setValue(160.0);
                spinLayerH2->setValue(10.0);   spinLayerVs2->setValue(450.0);
                spinLayerVs3->setValue(1200.0);
                spinWaveletFm->setValue(18.0);
            }
        }
        };

    connect(comboModelPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updatePreset);

    // 默认激活两层模型状态
    updatePreset(0);

    // 快速预览按钮联动
    connect(btnQuickCurve, &QPushButton::clicked, this, [=]() {
        double h1 = spinLayerH1->value(), vs1 = spinLayerVs1->value();
        double h2 = spinLayerH2->value(), vs2 = spinLayerVs2->value();
        double vs3 = spinLayerVs3->value();

        LayerModel model;
        // 如果第 2 层被禁用，底层自动按纯两层介质计算
        if (!spinLayerH2->isEnabled()) {
            model.H = { h1 };
            model.VS = { vs1, vs3 };
            model.VP = { vs1 * 2.0, vs3 * 2.0 };
            model.Rho = { 2000.0, 2000.0 };
        }
        else {
            model.H = { h1, h2 };
            model.VS = { vs1, vs2, vs3 };
            model.VP = { vs1 * 2.0, vs2 * 2.0, vs3 * 2.0 };
            model.Rho = { 1800.0, 2000.0, 2200.0 };
        }

        std::vector<double> freqs;
        for (double f = spinFmin->value(); f <= spinFreqMax->value(); f += 0.5) {
            freqs.push_back(f);
        }

        auto theoVel = RayleighForwardSolver::calcBaseDispersion(freqs, model);

        if (plotCurve1D) {
            plotCurve1D->setData(freqs, theoVel, QStringLiteral("理论频散曲线 (预览)"));
            plotCurve1D->graph(0)->setPen(QPen(Qt::red, 2.0));
            mainTabWidget->setCurrentWidget(plotCurve1D);
        }
        });

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
    btnStartGen->setObjectName(QStringLiteral("btnPrimary"));
    btnStartGen->setMinimumHeight(40);
    connect(btnStartGen, &QPushButton::clicked, this, &Pro_Seis_MASW::onStartDatasetGeneration);
    genLayout->addWidget(btnStartGen);

    genLayout->addStretch();


    // =========================================================================
    // 装配 Tab 页面到 DockWidget
    // =========================================================================
    dockTabs->addTab(pageSynthetic, QStringLiteral("🧪 理论正演"));
    dockTabs->addTab(pageDispersion, QStringLiteral("📊 频散分析"));
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
    applyApplicationTheme(this, property("_currentThemeIndex").toInt());

    // 自动切到第 1 页
    mainTabWidget->setCurrentIndex(0);
    // 【核心新增】：标记当前为外部实测数据
    m_dataSourceType = SourceExternal;
    if (comboInvLayers) comboInvLayers->setCurrentIndex(3); // 默认选第4项: 六层模型
}

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
    if (graphPicked) {
        graphPicked->setData(qf, qv);
        if (!qf.isEmpty()) {
            chkShowPicked->setChecked(true);
        }
        updateCurveComparisonView();
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
    // 1. 动态读取输入值
    double h1 = spinLayerH1->value();
    double vs1 = spinLayerVs1->value();
    double h2 = spinLayerH2->value();
    double vs2 = spinLayerVs2->value();
    double vs3 = spinLayerVs3->value();
    double fm = spinWaveletFm->value();

    // =========================================================
    // 【核心自适应】：根据第 2 层是否启用来构建物理地层模型
    // =========================================================
    LayerModel model;
    QString modelTypeStr;

    if (!spinLayerH2->isEnabled()) {
        // --- 纯两层模型 (单层覆盖基底) ---
        model.H = { h1 };
        model.VS = { vs1, vs3 };
        model.VP = { vs1 * 2.0, vs3 * 2.0 };
        model.Rho = { 2000.0, 2000.0 };
        modelTypeStr = QString("两层模型: H1=%1m, Vs1=%2m/s, 基底Vs=%3m/s").arg(h1).arg(vs1).arg(vs3);
    }
    else {
        // --- 三层模型 ---
        model.H = { h1, h2 };
        model.VS = { vs1, vs2, vs3 };
        model.VP = { vs1 * 2.0, vs2 * 2.0, vs3 * 2.0 };
        model.Rho = { 1800.0, 2000.0, 2200.0 };
        modelTypeStr = QString("三层模型: H=[%1,%2]m, Vs=[%3,%4,%5]m/s").arg(h1).arg(h2).arg(vs1).arg(vs2).arg(vs3);
    }

    // 【核心新增】：根据第2层是否启用来标记正演数据类型
    if (!spinLayerH2->isEnabled()) {
        m_dataSourceType = SourceSynthetic2L; // 理论两层
    }
    else {
        m_dataSourceType = SourceSynthetic3L; // 理论三层
    }

    // 2. 读取几何与频率范围
    double dt = spinDt->value() / 1000.0;
    double dx = spinDx->value();
    double x0 = spinOffset0->value();
    double fmin = spinFmin->value();
    double fmax = spinFreqMax->value();

    std::vector<double> freqs;
    for (double f = fmin; f <= fmax; f += 0.25) freqs.push_back(f);

    textLog->append(QStringLiteral("[%1] 正在进行理论面波正演计算 (%2)...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(modelTypeStr));

    // 3. 求解理论频散曲线 (自动处理 2 层或 3 层)
    auto theoVel = RayleighForwardSolver::calcBaseDispersion(freqs, model);

    // 4. 合成 96 道面波记录
    int nTraces = 96;
    std::vector<double> offsets;
    for (int i = 0; i < nTraces; ++i) offsets.push_back(x0 + i * dx);

    m_seismicData = RayleighForwardSolver::synthesizeSurfaceWaveGather(
        freqs, theoVel, dt, 1000, offsets, fm);

    // 5. 载入道集界面展示 (Tab 1)
    QWidget* plotWidget = createSeismicView(m_seismicData, QStringLiteral("理论合成面波剖面"), seismicViewContainer, static_cast<float>(dt));
    QLayout* layout = seismicViewContainer->layout();
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    layout->addWidget(plotWidget);
    applyApplicationTheme(this, property("_currentThemeIndex").toInt());
    mainTabWidget->setCurrentIndex(0);

    // 6. 将理论曲线更新至 Tab 3 (Plot1D)
    if (graphTheoretical) {
        QVector<double> qf(freqs.begin(), freqs.end());
        QVector<double> qv(theoVel.begin(), theoVel.end());
        graphTheoretical->setData(qf, qv);
        chkShowTheoretical->setChecked(true);
        updateCurveComparisonView();
    }

    textLog->append(QStringLiteral("[%1] 理论面波合成完成，成果已载入 Tab 1 和 Tab 3。")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    if (comboInvLayers) {
        if (!spinLayerH2->isEnabled()) {
            comboInvLayers->setCurrentIndex(0); // 选两层
        }
        else {
            comboInvLayers->setCurrentIndex(1); // 选三层
        }
    }
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

void Pro_Seis_MASW::updateCurveComparisonView()
{
    if (!plotCurve1D) return;

    plotCurve1D->rescaleAxes();
    double ySpan = plotCurve1D->yAxis->range().size();
    if (ySpan > 1e-4) {
        // 上下留 8% 呼吸感边距，避免点顶住边框
        plotCurve1D->yAxis->setRange(plotCurve1D->yAxis->range().lower - ySpan * 0.05,
            plotCurve1D->yAxis->range().upper + ySpan * 0.08);
    }
    plotCurve1D->replot();
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

void Pro_Seis_MASW::onRunInversionClicked()
{
    // 1. 检查是否有拾取的频散曲线
    if (m_pickedPoints.size() < 4) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("当前拾取的频散曲线点数不足（至少需要 4 个频点）！\n\n请在 Tab 2 进行【AI 一键拾取】或手动点选拾取。"));
        return;
    }

    // 2. 准备反演输入观测数据
    std::vector<double> freqs;
    std::vector<double> obsVel;
    freqs.reserve(m_pickedPoints.size());
    obsVel.reserve(m_pickedPoints.size());

    for (const auto& pt : m_pickedPoints) {
        freqs.push_back(pt.x());
        obsVel.push_back(pt.y());
    }

    textLog->append(QStringLiteral("[%1] 🚀 启动纯 C++ 原生 1D 横波速度反演 (Levenberg-Marquardt)...")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QElapsedTimer timer;
    timer.start(); 

    // 3. 配置反演参数 (与 Python 端保持一致)
    // =========================================================
    // 【核心升级】：根据数据来源与地质预设，自适应构建反演模型
    // =========================================================
    // =========================================================
    // 【核心实现】：根据用户在底栏下拉框选定的分层方案配置反演模型
    // =========================================================
    InversionParams params;
    params.vsMin = 100.0;
    params.vsMax = 1500.0;
    params.maxIter = 30;

    int layerMode = comboInvLayers->currentData().toInt(); // 获取层数 (2, 3, 4, 6, 8)
    QString modeDesc = comboInvLayers->currentText();

    if (layerMode == 2) {
        // --- 两层模型: 取 H1 作为第1层厚度，第2层为无限深半空间 ---
        double h1 = (spinLayerH1 && spinLayerH1->value() > 0.0) ? spinLayerH1->value() : 10.0;
        params.layerH = { h1 };
        params.lambdaReg = 0.001; // 两层突变模型，采用极弱平滑，保证界面陡峭
    }
    else if (layerMode == 3) {
        // --- 三层模型: 取 H1, H2 作为前两层厚度 ---
        double h1 = (spinLayerH1 && spinLayerH1->value() > 0.0) ? spinLayerH1->value() : 5.0;
        double h2 = (spinLayerH2 && spinLayerH2->value() > 0.0) ? spinLayerH2->value() : 10.0;
        params.layerH = { h1, h2 };
        params.lambdaReg = 0.005;
    }
    else if (layerMode == 4) {
        // --- 四层模型: 浅层工程 0~16m ---
        params.layerH = { 3.0, 5.0, 8.0 };
        params.lambdaReg = 0.015;
    }
    else if (layerMode == 8) {
        // --- 八层模型: 深部高密 0~44m ---
        params.layerH = { 1.5, 2.5, 4.0, 6.0, 8.0, 10.0, 12.0 };
        params.lambdaReg = 0.05;
    }
    else {
        // --- 六层模型 (默认推荐): 经典 0~30m 勘察剖面 ---
        params.layerH = { 2.0, 3.0, 5.0, 8.0, 12.0 };
        params.lambdaReg = 0.03;
    }

    textLog->append(QStringLiteral("[%1] 🚀 启动 1D 速度反演 | 用户选定方案: 【%2】")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(modeDesc));

    // 4. 执行纯 C++ 原生非线性反演
    InversionResult res = RayleighInversionSolver::runInversion(freqs, obsVel, params);
    qint64 elapsedMs = timer.elapsed();
    QApplication::restoreOverrideCursor();

    if (!res.success) {
        QMessageBox::critical(this, QStringLiteral("反演失败"),
            QStringLiteral("输入数据超出当前速度边界，或反演未能收敛。请检查拾取曲线的速度范围和所选模型层数。"));
        return;
    }

    // 5. 自动导出保存 Inverted_Vs_Model.txt (保证成果有存档)
    QString outPath = QCoreApplication::applicationDirPath() + "/models/Inverted_Vs_Model.txt";
    QDir dir(QFileInfo(outPath).absolutePath());
    if (!dir.exists()) dir.mkpath(".");

    QFile file(outPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "# Layer\tThickness(m)\tDepth_Top(m)\tVs(m/s)\tVp(m/s)\tDensity(kg/m3)\n";
        double topZ = 0.0;
        int nLayers = res.vs.size();
        for (int i = 0; i < nLayers; ++i) {
            double thk = (i < params.layerH.size()) ? params.layerH[i] : 0.0;
            out << (i + 1) << "\t"
                << QString::number(thk, 'f', 2) << "\t"
                << QString::number(topZ, 'f', 2) << "\t"
                << QString::number(res.vs[i], 'f', 2) << "\t"
                << QString::number(res.vs[i] * 2.0, 'f', 2) << "\t2000.0\n";
            topZ += thk;
        }
        file.close();
    }

    // 6. 直接调用 displayInversionModel 绘制 Tab 4 的阶梯剖面与表格
    displayInversionModel(outPath);

    // 7. 同步将反演拟合的理论黑线回传至 Tab 3，实现拟合度可视化质检
    if (graphInverted) {
        QVector<double> qf(res.freqs.begin(), res.freqs.end());
        QVector<double> qv(res.calcVel.begin(), res.calcVel.end());
        graphInverted->setData(qf, qv);
        graphInverted->setName(QStringLiteral("反演拟合曲线 (RMSE=%1 m/s)").arg(res.rmse, 0, 'f', 2));
        chkShowInverted->setChecked(true);

        // 刷新底栏 RMSE 残差看板
        lblCurveMisfit->setText(QString("拟合残差 RMSE: <font color='#10b981'><b>%1 m/s</b></font>").arg(res.rmse, 0, 'f', 2));
        updateCurveComparisonView();
    }

    // 8. 界面与日志提示
    textLog->append(QStringLiteral("[%1] 🎉 C++ 原生反演成功收敛！迭代次数: %2 轮, 耗时: %3 ms, 拟合误差 RMSE: %4 m/s。")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(res.iterations)
        .arg(elapsedMs)
        .arg(res.rmse, 0, 'f', 2));

    QMessageBox::information(this, QStringLiteral("反演成功"),
        QStringLiteral("纯 C++ 1D 速度结构反演已成功收敛！\n\n"
            "• 迭代次数: %1 次\n"
            "• 运算耗时: %2 ms (毫秒级原生计算)\n"
            "• 拟合误差 RMSE: %3 m/s\n"
            "• 成果已同步至 Tab 3 (拟合度)、Tab 4 (阶梯图/表格) 与 Tab 5 (二维模拟剖面)")
        .arg(res.iterations).arg(elapsedMs).arg(res.rmse, 0, 'f', 2));

    // 自动切到 Tab 4 查看成果
    mainTabWidget->setCurrentWidget(inversionContainer);
}

void Pro_Seis_MASW::displayInversionModel(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

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

        QStringList tokens = line.simplified().split(QLatin1Char(' '));
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

    if (layers.isEmpty()) return;

    m_vsModelLayers.clear();
    m_vsModelLayers.reserve(layers.size());
    for (const auto& layer : layers) {
        m_vsModelLayers.append({ layer.depthTop, layer.thk, layer.vs });
    }

    // =========================================================
    // 1. 刷新右侧表格
    // =========================================================
    tableVsModel->setRowCount(layers.size());
    double maxDepth = 0.0;
    for (int i = 0; i < layers.size(); ++i) {
        const auto& l = layers[i];
        tableVsModel->setItem(i, 0, new QTableWidgetItem(QString::number(l.layer)));
        tableVsModel->setItem(i, 1, new QTableWidgetItem(l.thk > 0 ? QString::number(l.thk, 'f', 1) : QStringLiteral("无限")));
        tableVsModel->setItem(i, 2, new QTableWidgetItem(QString::number(l.depthTop, 'f', 1)));
        tableVsModel->setItem(i, 3, new QTableWidgetItem(QString::number(l.vs, 'f', 1)));
        tableVsModel->setItem(i, 4, new QTableWidgetItem(QString::number(l.vp, 'f', 1)));

        for (int c = 0; c < 5; ++c) {
            tableVsModel->item(i, c)->setTextAlignment(Qt::AlignCenter);
        }

        double zBottom = (l.thk > 0) ? (l.depthTop + l.thk) : (l.depthTop + 12.0);
        maxDepth = std::max(maxDepth, zBottom);
    }

    // =========================================================
    // 2. 【核心修复】：使用 QCPCurve 代替 QCPGraph 绘制垂直阶梯图
    // QCPCurve 严格按点的加入顺序连接，绝对不会按 X 坐标重排导致折线交叉！
    // =========================================================
    plotVsProfile->clearPlottables(); // 清空原有图层

    QCPCurve* vsStepCurve = new QCPCurve(plotVsProfile->xAxis, plotVsProfile->yAxis);
    QVector<QCPCurveData> curveData;
    int ptIdx = 0;

    double vsMinVal = 1e9, vsMaxVal = -1e9;

    for (int i = 0; i < layers.size(); ++i) {
        const auto& l = layers[i];
        double zTop = l.depthTop;
        double zBottom = (l.thk > 0) ? (zTop + l.thk) : (zTop + 12.0);

        vsMinVal = std::min(vsMinVal, l.vs);
        vsMaxVal = std::max(vsMaxVal, l.vs);

        // 点 1: 顶面 (vs, zTop)
        curveData.append(QCPCurveData(ptIdx++, l.vs, zTop));
        // 点 2: 底面 (vs, zBottom)
        curveData.append(QCPCurveData(ptIdx++, l.vs, zBottom));
        // 层间界面按深度水平连接，避免把速度突变画成斜坡。
        if (i + 1 < layers.size()) {
            curveData.append(QCPCurveData(ptIdx++, layers[i + 1].vs, zBottom));
        }
    }

    vsStepCurve->data()->set(curveData, true); // true 表示数据已按时间/顺序排好
    vsStepCurve->setPen(QPen(QColor(2, 132, 199), 2.5)); // 经典亮蓝色阶梯线
    vsStepCurve->setName(QStringLiteral("反演地层模型 (Inverted Vs)"));

    // 坐标轴设定
    plotVsProfile->xAxis->setLabel(QStringLiteral("横波速度 Vs (m/s)"));
    plotVsProfile->yAxis->setLabel(QStringLiteral("地下深度 Depth (m)"));
    plotVsProfile->yAxis->setRangeReversed(true); // 深度向下为正

    plotVsProfile->xAxis->setRange(vsMinVal - 40.0, vsMaxVal + 40.0);
    plotVsProfile->yAxis->setRange(0.0, maxDepth);

    plotVsProfile->replot();

    // 3. 摘要更新
    lblVsSummary->setText(QStringLiteral("已成功载入: %1 层模型 | 探测最大深度: %2 m | 基底 Vs: %3 m/s")
        .arg(layers.size()).arg(maxDepth, 0, 'f', 1).arg(layers.last().vs, 0, 'f', 1));

    update2DVsSection();
    mainTabWidget->setCurrentWidget(inversionContainer);
}

void Pro_Seis_MASW::update2DVsSection()
{
    if (m_vsModelLayers.isEmpty() || !plotVsSection || !spinSectionLength ||
        !spinSectionNx || !spinSectionNz || !spinSectionVariation || !comboSectionPalette) {
        if (lblSectionStatus) {
            lblSectionStatus->setText(QStringLiteral("请先在页面 4 完成 1D 反演或载入有效模型。"));
        }
        return;
    }

    const int nx = spinSectionNx->value();
    const int nz = spinSectionNz->value();
    const double lineLength = spinSectionLength->value();
    const double variation = spinSectionVariation->value() / 100.0;
    constexpr double Pi = 3.14159265358979323846;

    double totalFiniteDepth = 0.0;
    double minLayerThickness = std::numeric_limits<double>::max();
    for (const auto& layer : m_vsModelLayers) {
        if (layer.thickness > 0.0) {
            totalFiniteDepth = std::max(totalFiniteDepth, layer.topDepth + layer.thickness);
            minLayerThickness = std::min(minLayerThickness, layer.thickness);
        }
    }
    if (totalFiniteDepth <= 0.0) totalFiniteDepth = 20.0;
    if (minLayerThickness == std::numeric_limits<double>::max()) minLayerThickness = totalFiniteDepth / 4.0;

    const auto& lastLayer = m_vsModelLayers.last();
    const double maxDepth = lastLayer.thickness > 0.0
        ? totalFiniteDepth
        : lastLayer.topDepth + std::max(12.0, totalFiniteDepth * 0.35);
    const double interfaceRelief = minLayerThickness * 0.18 * (variation / 0.25);

    if (!vsSectionColorMap) {
        vsSectionColorMap = new QCPColorMap(plotVsSection->xAxis, plotVsSection->yAxis);
        vsSectionColorMap->setColorScale(vsSectionColorScale);
        vsSectionColorMap->setInterpolate(true);
    }

    vsSectionColorMap->data()->setSize(nx, nz);
    vsSectionColorMap->data()->setRange(QCPRange(0.0, lineLength), QCPRange(0.0, maxDepth));

    QVector<double> interfaceX(nx);
    for (int ix = 0; ix < nx; ++ix) {
        interfaceX[ix] = lineLength * ix / (nx - 1.0);
    }

    double minVs = std::numeric_limits<double>::max();
    double maxVs = std::numeric_limits<double>::lowest();
    const double xScale = std::max(1.0, lineLength);
    const double zScale = std::max(1.0, maxDepth);

    for (int iz = 0; iz < nz; ++iz) {
        const double depth = maxDepth * iz / (nz - 1.0);
        for (int ix = 0; ix < nx; ++ix) {
            const double x = interfaceX[ix];
            const double xNorm = x / xScale;
            const double zNorm = depth / zScale;
            const double interfaceShift = interfaceRelief * std::sin(2.0 * Pi * xNorm);
            const double modelDepth = std::max(0.0, depth - interfaceShift);

            int layerIndex = 0;
            for (int i = 0; i + 1 < m_vsModelLayers.size(); ++i) {
                const auto& layer = m_vsModelLayers[i];
                if (modelDepth >= layer.topDepth + layer.thickness) {
                    layerIndex = i + 1;
                }
                else {
                    break;
                }
            }

            double structure = 0.55 * std::sin(2.0 * Pi * xNorm + 1.4 * zNorm)
                + 0.25 * std::cos(4.0 * Pi * xNorm - 2.2 * zNorm);
            const double anomalyA = std::exp(-((xNorm - 0.28) * (xNorm - 0.28) / 0.018
                + (zNorm - 0.42) * (zNorm - 0.42) / 0.035));
            const double anomalyB = std::exp(-((xNorm - 0.72) * (xNorm - 0.72) / 0.022
                + (zNorm - 0.72) * (zNorm - 0.72) / 0.045));
            structure = std::clamp(structure + 0.55 * anomalyA - 0.55 * anomalyB, -1.0, 1.0);

            const double vs = std::max(1.0, m_vsModelLayers[layerIndex].vs * (1.0 + variation * structure));
            vsSectionColorMap->data()->setCell(ix, iz, vs);
            minVs = std::min(minVs, vs);
            maxVs = std::max(maxVs, vs);
        }
    }

    vsSectionColorMap->setGradient(getScientificGradient(comboSectionPalette->currentText()));
    vsSectionColorMap->setDataRange(QCPRange(minVs, maxVs > minVs ? maxVs : minVs + 1.0));

    plotVsSection->clearGraphs();
    for (int boundary = 1; boundary < m_vsModelLayers.size(); ++boundary) {
        QVector<double> boundaryDepth(nx);
        const double baseDepth = m_vsModelLayers[boundary].topDepth;
        for (int ix = 0; ix < nx; ++ix) {
            const double xNorm = interfaceX[ix] / xScale;
            boundaryDepth[ix] = baseDepth + interfaceRelief * std::sin(2.0 * Pi * xNorm);
        }
        QCPGraph* boundaryGraph = plotVsSection->addGraph();
        boundaryGraph->setData(interfaceX, boundaryDepth);
        boundaryGraph->setPen(QPen(QColor(248, 250, 252, 180), 1.2, Qt::DashLine));
        boundaryGraph->setName(QStringLiteral("模拟层界面"));
    }

    plotVsSection->xAxis->setRange(0.0, lineLength);
    plotVsSection->yAxis->setRangeReversed(true);
    plotVsSection->yAxis->setRange(0.0, maxDepth);
    plotVsSection->replot();

    btnGenerateSection->setEnabled(true);
    lblSectionStatus->setText(QStringLiteral("由 %1 层 1D 模型生成 | 网格 %2 × %3 | 横向变化 %4% | Vs 范围 %5–%6 m/s")
        .arg(m_vsModelLayers.size()).arg(nx).arg(nz)
        .arg(spinSectionVariation->value(), 0, 'f', 0)
        .arg(minVs, 0, 'f', 0).arg(maxVs, 0, 'f', 0));
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
    // =========================================================
    // 1. 创建独立帮助手册窗口 (QDialog)
    // =========================================================
    QDialog* helpDialog = new QDialog(this);
    helpDialog->setAttribute(Qt::WA_DeleteOnClose);
    helpDialog->setWindowTitle(QStringLiteral("SeisTool-MASW 用户手册与理论指南 v1.0"));
    helpDialog->resize(920, 680);
    helpDialog->setMinimumSize(780, 500);
    helpDialog->setStyleSheet(
        "QDialog { background-color: #f4f5f7; color: #222222; }"
        "QTextBrowser { background-color: #ffffff; color: #222222; border: 1px solid #c8cdd4; }"
        "QPushButton { background-color: #0072bd; color: #ffffff; border: 1px solid #0063a5; border-radius: 4px; padding: 6px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: #005a9c; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(helpDialog);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // 2. 核心分页容器 (QTabWidget)
    QTabWidget* helpTabs = new QTabWidget(helpDialog);
    helpTabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #c8cdd4; background-color: #ffffff; border-radius: 4px; }"
        "QTabBar::tab { background: #e7e9ed; color: #374151; padding: 9px 16px; margin-right: 2px; border: 1px solid #c8cdd4; font-weight: 600; font-size: 12px; }"
        "QTabBar::tab:selected { background: #ffffff; color: #0072bd; border-bottom: 2px solid #0072bd; }"
        "QTabBar::tab:hover:!selected { background: #f0f2f5; color: #111827; }"
    );

    // HTML 基础样式模板
    QString htmlHead =
        "<style>"
        "body { font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; font-size: 14px; color: #24292f; line-height: 1.65; background-color: #ffffff; padding: 12px; }"
        "h2 { color: #005a9c; border-bottom: 1px solid #d0d7de; padding-bottom: 6px; font-size: 18px; margin-top: 8px; }"
        "h3 { color: #0072bd; font-size: 15px; margin-top: 16px; margin-bottom: 5px; }"
        "b, strong { color: #111827; }"
        "code { background-color: #f1f3f5; color: #8a3ffc; padding: 2px 6px; border-radius: 3px; font-family: 'Consolas', monospace; font-size: 13px; }"
        "pre { background-color: #f6f8fa; border: 1px solid #d0d7de; border-radius: 4px; padding: 9px; color: #24292f; font-family: 'Consolas', monospace; font-size: 13px; }"
        "table { width: 100%; border-collapse: collapse; margin-top: 8px; margin-bottom: 12px; }"
        "th { background-color: #eaf2f8; color: #1f2937; font-weight: bold; padding: 7px 10px; border: 1px solid #c8cdd4; text-align: left; }"
        "td { padding: 7px 10px; border: 1px solid #c8cdd4; color: #24292f; }"
        "tr:nth-child(even) { background-color: #f6f8fa; }"
        ".tip-box { background-color: #eff6fc; border-left: 4px solid #0072bd; padding: 9px 12px; margin: 10px 0; border-radius: 2px; }"
        ".warn-box { background-color: #fff7e6; border-left: 4px solid #d99000; padding: 9px 12px; margin: 10px 0; border-radius: 2px; }"
        "ul, ol { margin-top: 4px; margin-bottom: 8px; padding-left: 22px; }"
        "li { margin-bottom: 4px; }"
        "</style>";

    // =========================================================================
    // 【Tab 1: 概述与系统工作流】
    // =========================================================================
    QTextBrowser* tb1 = new QTextBrowser(helpTabs);
    tb1->setOpenExternalLinks(true);
    tb1->setHtml(htmlHead + QStringLiteral(
        "<h2>一、 系统概述 (System Overview)</h2>"
        "<p><b>SeisTool-MASW</b> 是一款专为近地表工程物探勘察研发的专业级多道面波分析系统（MASW，Multichannel Analysis of Surface Waves）。系统突破了传统软件各环节割裂的弊端，实现了从原始波形到速度反演的完整闭环。</p>"
        "<div class='tip-box'>"
        "<b>适用应用场景：</b>"
        "<ul>"
        "<li><b>浅层岩土勘察</b>：基岩埋深探测、地层分层与断层破碎带定位；</li>"
        "<li><b>城市隐患排查</b>：道路/堤坝内部空洞、脱空区及溶洞病害排查；</li>"
        "<li><b>工程抗震评价</b>：建筑抗震场地类别划分（V<sub>s30</sub> 等效剪切波速计算）；</li>"
        "<li><b>科研与教学</b>：面波高阶模态识别、理论频散特征分析及数值验证。</li>"
        "</ul>"
        "</div>"
        "<h2>二、 五个主窗口工作流程 (5-Stage Pipeline)</h2>"
        "<p>系统主界面严格遵循地球物理勘察标准生命周期构建：</p>"
        "<ol>"
        "<li><b>Tab 1 原始道集 (Shot Gather)</b>：<br>"
        "负责外场实测 SEG-Y 数据解码、质量检验（QC）、坏道识别及理论合成炮集预览。支持波形起伏线（Wiggle）与变密度热力图双层叠加显示，内置 LOD 视口优化。</li>"
        "<li><b>Tab 2 频散能量谱 (Dispersion Map)</b>：<br>"
        "系统的核心计算与交互区。支持将时空波场变换为相速度-频率能量谱，提供四大成像算子，支持 Snap-to-Peak 自动寻峰吸附及 AI 一键智能拾取。</li>"
        "<li><b>Tab 3 频散曲线 (Extracted Curves)</b>：<br>"
        "期刊级 1D 频散曲线对比展示区（Plot1D）。将实测提取的相速度离散点与理论计算红线叠合比对，支持导出工业标准反演 ASCII 文本。</li>"
        "<li><b>Tab 4 速度结构 (Vs Profile)</b>：<br>"
        "地质最终成果交付区。呈现向下递增的垂直阶梯速度剖面（Step Profile）与分层物理参数表，支持原生 C++ Levenberg-Marquardt 阻尼非线性反演。</li>"
        "<li><b>Tab 5 二维 Vs 剖面 (2D Section)</b>：<br>"
        "将当前 1D 模型沿测线方向扩展为二维初始演示模型，可调测线长度、网格密度、色带和横向扰动强度；横向扰动为合成演示，不代表二维反演成果。</li>"
        "</ol>"
    ));
    helpTabs->addTab(tb1, QStringLiteral("📖 概述与工作流"));

    // =========================================================================
    // 【Tab 2: 四大频散算法与拾取指南】
    // =========================================================================
    QTextBrowser* tb2 = new QTextBrowser(helpTabs);
    tb2->setHtml(htmlHead + QStringLiteral(
        "<h2>一、 四大频散能量谱成像算子对比</h2>"
        "<table>"
        "<tr><th>算法名称</th><th>数学特征</th><th>主要优势</th><th>适用场景与局限</th></tr>"
        "<tr>"
        "<td><b>移相法 (Phase Shift)</b><br>Park et al., 1998</td>"
        "<td>逐道傅里叶振幅纯相位归一化，空间复数相移干涉叠加：<br><code>E = |Σ P(x,f)·e^(i·2πf·x/v)| / N</code></td>"
        "<td>高低频能量极其均衡；抗几何扩散与地层吸收衰减能力最强，对非等间距排列宽容度高。</td>"
        "<td><b>常规工程生产主力算子</b>（推荐默认使用）；能量条带宽度中等。</td>"
        "</tr>"
        "<tr>"
        "<td><b>Capon 高分辨率 (MVDR)</b><br>Capon, 1969</td>"
        "<td>双向空间平滑满秩协方差求逆，自适应抑制旁瓣干扰：<br><code>E = 1 / (a^H · R^(-1) · a)</code></td>"
        "<td><b>能量脊线极度纤细</b>，条带宽度约为移相法的 1/3；基阶与高阶分界极其分明。</td>"
        "<td>适合精细分层与复杂场地；单炮记录必须依赖子阵列空间平滑保证满秩。</td>"
        "</tr>"
        "<tr>"
        "<td><b>倾斜叠加法 (τ-p 变换)</b><br>McMechan & Yedlin, 1981</td>"
        "<td>时空域线性动校正（LMO）截距时间叠加，沿 τ 做 1D-FFT：<br><code>u(τ,p) = Σ u(x, τ+p·x), p=1/v</code></td>"
        "<td>最纯粹的物理走时干涉叠加；直观反映波场真实能量密度。</td>"
        "<td>未做振幅均衡，受近道强能量主导，低频与高频端衰减较快。</td>"
        "</tr>"
        "<tr>"
        "<td><b>F-K 变换法 (2D-FFT)</b><br>Yilmaz, 1987</td>"
        "<td>时空二维双重傅里叶变换映射至 (f, k)，再利用 <code>v = 2πf/k</code> 映射到相速度轴。</td>"
        "<td>计算速度最快，纯矩阵变换。</td>"
        "<td>受空间检波器孔径截断影响较大；在低波数区速度离散度非线性拉伸。</td>"
        "</tr>"
        "</table>"

        "<h2>二、 频散曲线拾取机制</h2>"
        "<h3>1. 交互点选 + 局部极大值自动吸附 (Snap-to-Peak)</h3>"
        "<ul>"
        "<li><b>操作方法</b>：在 Tab 2 底栏勾选 <code>[√] 拾取模式</code>，鼠标在红黄色能量脊线附近单机左键即可；</li>"
        "<li><b>算法原理</b>：算法以点击位置为中心，在垂直速度方向（±15 个网格）自适应搜索物理能量最大值点（Peak），自动吸附到位，<b>彻底消除人工手抖误差</b>；</li>"
        "<li><b>编辑技巧</b>：点错时点击 <code>【撤销点】</code>；同一频点重复点击会自动覆盖旧点；快捷键 <code>【清空曲线】</code> 可重新开始。</li>"
        "</ul>"
        "<h3>2. 🤖 ONNX Runtime AI 一键智能拾取</h3>"
        "<ul>"
        "<li>系统内置专用多尺度空洞残差卷积网络（<code>DispersionRidgeNet</code>），仅 7.9 万参数；</li>"
        "<li>点击 <code>【🤖 AI 一键拾取】</code>，后台调用本地 ONNX 引擎进行毫秒级端到端骨架分割，直接生成光滑连续的基阶相速度曲线。</li>"
        "</ul>"

        "<h2>三、 谱图显示模式控制</h2>"
        "<ul>"
        "<li><b>双模式归一化</b>：底栏支持 <code>按频率归一化 (推荐)</code>（每列最大值=1.0，整条曲线清晰高亮）与 <code>全局归一化</code>（保留激发能量在主频的聚集特征）毫秒级切换；</li>"
        "<li><b>平滑度切换</b>：右键菜单可勾选 <code>平滑插值 (Smooth Blur)</code> 开启双线性平滑云雾图，或取消勾选呈现 MATLAB 风格的清晰离散网格色块。</li>"
        "</ul>"
    ));
    helpTabs->addTab(tb2, QStringLiteral("⚡ 频散算法与拾取"));

    // =========================================================
    // 【Tab 3: 理论正演与 1D 速度反演】
    // =========================================================
    QTextBrowser* tb3 = new QTextBrowser(helpTabs);
    tb3->setHtml(htmlHead + QStringLiteral(
        "<h2>一、 层状介质理论频散正演模拟 (Forward Modeling)</h2>"
        "<p>系统在控制面板提供了强大的数值仿真引擎，支持双层及三层地质模型面波记录合成：</p>"
        "<ul>"
        "<li><b>Schwab-Knopoff 传递矩阵求根算法</b>：<br>"
        "建立多层弹性半空间自由应力边界与位移连续性超越特征方程 <code>F(c, ω) = 0</code>，采用双曲正切消指数增长技术 <code>tanh(k·r·d)</code>，从高频表层渐近线（c ≈ 0.92 Vs<sub>1</sub>）逆序追踪求根，<b>彻底根治高频溢出</b>；</li>"
        "<li><b>物理频散波场合成机制</b>：<br>"
        "提取雷克子波振幅谱 A(f)，引入理论相速度相移因子 <code>exp(-i·2πf·x / c(f))</code>，经逆傅里叶变换（IFFT）生成具有真实频散延迟的多道炮集。</li>"
        "</ul>"

        "<h2>二、 1D 横波速度结构反演 (Vs Inversion)</h2>"
        "<div class='tip-box'>"
        "<b>地学目标：</b>从实测提取的相速度频散曲线 (f, v<sub>R</sub>)，反演求解地下各层的真实厚度 H 与横波速度 V<sub>s</sub> 分层结构。"
        "</div>"
        "<h3>1. 目标泛函与正则化模型</h3>"
        "<p>反演系统求解以下带 Tikhonov 一阶平滑粗糙度约束的非线性最小二乘目标方程：</p>"
        "<pre>Φ(m) = 1/2 · ||v_obs - v_calc(m)||^2 + 1/2 · λ · ||L · m||^2</pre>"
        "<ul>"
        "<li><b>数据残差项</b>：保证理论计算值与实测拾取点紧密吻合；</li>"
        "<li><b>正则化项 ||L·m||²</b>：一阶差分算子，约束相邻层速度突变，<b>防止反演解出现剧烈非物理震荡（锯齿化）</b>；</li>"
        "<li><b>平滑因子 λ (默认 0.02)</b>：数据质量高/界线分明时可调小（如 0.005）；噪声大时调大（如 0.05）。</li>"
        "</ul>"

        "<h3>2. 求解引擎 (C++ Levenberg-Marquardt)</h3>"
        "<ul>"
        "<li>系统采用 Eigen 密集矩阵库，通过数值微小扰动显式计算雅可比敏感度矩阵 J，建立正规方程：<br>"
        "<code>(J^T·J + λ·L^T·L + μ·I) · Δm = J^T·(v_obs - v_calc) - λ·L^T·L·m</code></li>"
        "<li>单步限制更新步长不超过 15%，施加 [100, 1500] m/s 物理有界约束，收敛耗时仅需 <b>20 ~ 50 毫秒</b>；</li>"
        "<li>反演完成后，Tab 4 更新标准垂直阶梯图与分层参数表，Tab 5 同步生成由 1D 结果扩展的二维演示初始模型。</li>"
        "</ul>"
    ));
    helpTabs->addTab(tb3, QStringLiteral("🧪 正演模拟与反演"));

    // =========================================================
    // 【Tab 4: 快捷键、参数规范与 FAQ】
    // =========================================================
    QTextBrowser* tb4 = new QTextBrowser(helpTabs);
    tb4->setHtml(htmlHead + QStringLiteral(
        "<h2>一、 常用快捷键与鼠标交互 (Hotkeys & Mouse)</h2>"
        "<table>"
        "<tr><th>操作 / 快捷键</th><th>所在区域</th><th>功能描述</th></tr>"
        "<tr><td><b>Ctrl + O</b></td><td>全局</td><td>弹出标准文件对话框打开 SEGY 数据</td></tr>"
        "<tr><td><b>直接文件拖拽</b></td><td>主窗口任意位置</td><td>将桌面 .sgy / .segy 文件拖入窗口秒级载入</td></tr>"
        "<tr><td><b>鼠标滚轮</b></td><td>所有图表区</td><td>以鼠标指针为中心进行视野无级缩放</td></tr>"
        "<tr><td><b>鼠标左键拖拽</b></td><td>所有图表区</td><td>平移坐标系视野（拾取模式开启时自动保护锁定）</td></tr>"
        "<tr><td><b>Shift + 左键拖拽</b></td><td>Tab 3 / Tab 4</td><td>框选局部曲线数据（用于时频分析与局域导出）</td></tr>"
        "<tr><td><b>鼠标右键单击</b></td><td>Tab 1 / Tab 2</td><td>弹出快捷菜单（重置视图、切换样式、导出图像/SEGY）</td></tr>"
        "</table>"

        "<h2>二、 常见地学参数设置经验准则 (Best Practices)</h2>"
        "<ul>"
        "<li><b>采样间隔 (dt)</b>：必须与原始文件绝对相符（通常为 0.5ms、1.0ms 或 2.0ms），dt 错误会导致频率轴等比缩放失真；</li>"
        "<li><b>道间距 (dx)</b>：决定了空间假频上限（空间奈奎斯特极限：λ_min = 2·dx）。若 dx=1m，最高有效分析波长为 2m；</li>"
        "<li><b>最大反演深度准则 (半波长法则)</b>：<br>"
        "面波最大探测深度通常约为最大波长的一半：<code>Z_max ≈ λ_max / 2 = v_R(f_min) / (2 · f_min)</code>。<br>"
        "例如最长相速度为 400 m/s，分析下限为 5 Hz，则探测深度极限约为 <code>400 / (2×5) = 40 米</code>。据此设定分层总深度最为科学。</li>"
        "</ul>"

        "<h2>三、 常见故障排查 (Troubleshooting FAQ)</h2>"
        "<div class='warn-box'>"
        "<b>Q1：为什么导入数据后计算出的频散谱全是蓝色背景，看不到红带？</b><br>"
        "<b>A</b>：请检查观测参数中的 <code>采样间隔 (dt)</code> 是否设置正确；检查速度扫描区间 <code>[Vmin, Vmax]</code> 是否包含了真实相速度；在 Tab 2 底栏确认是否选择了 <code>按频率归一化 (每列)</code>。"
        "</div>"
        "<div class='warn-box'>"
        "<b>Q2：为什么拖入 SEGY 文件时出现鼠标禁止图标 (🚫)？</b><br>"
        "<b>A</b>：由于 Windows 的 UIPI 安全隔离机制，如果 Visual Studio 是“以管理员身份运行”启动的，系统会禁止普通资源管理器向其拖入文件。请直接双击运行编译出的 .exe 文件测试拖拽。"
        "</div>"
        "<div class='warn-box'>"
        "<b>Q3：反演时提示拟合误差较大（RMSE 偏高）如何改善？</b><br>"
        "<b>A</b>：① 检查拾取曲线中是否混入了高阶模态或异常抖动坏点；② 在反演参数中根据半波长法则合理调整地层总深度；③ 适当调整平滑正则化系数 λ（噪声大时调大至 0.05）。"
        "</div>"
    ));
    helpTabs->addTab(tb4, QStringLiteral("⌨️ 快捷键与 FAQ"));

    mainLayout->addWidget(helpTabs, 1);

    // =========================================================
    // 3. 底部对话框控制按钮
    // =========================================================
    QHBoxLayout* bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(4, 4, 4, 4);

    QLabel* lblCopy = new QLabel(QStringLiteral("SeisTool-MASW © 2026 | 基于 Qt5/6, Eigen3, QCustomPlot & ONNX Runtime 构建"), helpDialog);
    lblCopy->setStyleSheet("color: #64748b; font-size: 11px; border: none;");

    QPushButton* btnClose = new QPushButton(QStringLiteral("关闭手册 (Close)"), helpDialog);
    btnClose->setFixedWidth(120);
    btnClose->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; border-radius: 4px; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(btnClose, &QPushButton::clicked, helpDialog, &QDialog::accept);

    bottomLayout->addWidget(lblCopy);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnClose);

    mainLayout->addLayout(bottomLayout);

    // 弹出非模态或模态窗口
    helpDialog->exec();
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
