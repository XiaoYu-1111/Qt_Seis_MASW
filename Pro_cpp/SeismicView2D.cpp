#include "Pro_h/SeismicView2D.h"
#include "Pro_h/QCPSeismicWiggle.h"
#include "Pro_h/SeismicIO.h"
#include "qcustomplot.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMenu>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <cmath>
#include <algorithm>

// =========================================================
// 辅助函数：预设地学色标生成器 (模块化，消除臃肿代码)
// =========================================================
static QCPColorGradient getSeismicPresetGradient(const QString& type, bool invert)
{
    QCPColorGradient grad;
    grad.clearColorStops();

    if (type == "Jet" || type == "Jet (Classic)") grad = QCPColorGradient::gpJet;
    else if (type == "Terrain") {
        grad.setColorStopAt(0.0, QColor(0, 60, 170));     grad.setColorStopAt(0.25, QColor(0, 180, 220));
        grad.setColorStopAt(0.45, QColor(240, 240, 120)); grad.setColorStopAt(0.6, QColor(50, 200, 50));
        grad.setColorStopAt(0.8, QColor(120, 100, 60));   grad.setColorStopAt(1.0, QColor(250, 250, 250));
    }
    else if (type == "Seismic" || type == "Seismic (RdBu)") {
        grad.setColorStopAt(0.0, QColor(0, 0, 180)); grad.setColorStopAt(0.5, Qt::white); grad.setColorStopAt(1.0, QColor(180, 0, 0));
    }
    else if (type == "Grayscale") {
        grad.setColorStopAt(0.0, Qt::black); grad.setColorStopAt(1.0, Qt::white);
    }
    else if (type == "Hot") grad = QCPColorGradient::gpHot;
    else if (type == "Cold") grad = QCPColorGradient::gpCold;
    else if (type == "Coolwarm") {
        grad.setColorStopAt(0.0, QColor(59, 76, 192)); grad.setColorStopAt(0.5, QColor(221, 221, 221)); grad.setColorStopAt(1.0, QColor(180, 4, 38));
    }
    else if (type == "Viridis" || type == "Viridis (Recommended)") {
        grad.setColorStopAt(0.0, QColor(68, 1, 84));    grad.setColorStopAt(0.2, QColor(72, 35, 116));
        grad.setColorStopAt(0.4, QColor(64, 103, 138)); grad.setColorStopAt(0.6, QColor(53, 183, 121));
        grad.setColorStopAt(0.8, QColor(143, 215, 68)); grad.setColorStopAt(1.0, QColor(253, 231, 37));
    }
    else if (type == "Plasma") {
        grad.setColorStopAt(0.0, QColor(13, 8, 135));   grad.setColorStopAt(0.25, QColor(106, 0, 168));
        grad.setColorStopAt(0.5, QColor(187, 55, 84));  grad.setColorStopAt(0.75, QColor(249, 142, 9));
        grad.setColorStopAt(1.0, QColor(240, 249, 33));
    }
    else if (type == "Turbo" || type == "Turbo (Better Jet)") {
        grad.setColorStopAt(0.0, QColor(48, 18, 59));   grad.setColorStopAt(0.1, QColor(70, 107, 227));
        grad.setColorStopAt(0.2, QColor(40, 187, 235)); grad.setColorStopAt(0.35, QColor(50, 241, 151));
        grad.setColorStopAt(0.5, QColor(164, 252, 60)); grad.setColorStopAt(0.7, QColor(237, 208, 58));
        grad.setColorStopAt(0.85, QColor(253, 128, 40)); grad.setColorStopAt(1.0, QColor(122, 4, 3));
    }
    else if (type == "Inferno") {
        grad.setColorStopAt(0.0, QColor(0, 0, 4));      grad.setColorStopAt(0.25, QColor(87, 16, 109));
        grad.setColorStopAt(0.5, QColor(187, 55, 84));  grad.setColorStopAt(0.75, QColor(249, 142, 9));
        grad.setColorStopAt(1.0, QColor(252, 255, 164));
    }
    else if (type == "PuOr" || type == "PuOr (Diverging)") {
        grad.setColorStopAt(0.0, QColor(127, 59, 8));   grad.setColorStopAt(0.25, QColor(253, 184, 99));
        grad.setColorStopAt(0.5, Qt::white);            grad.setColorStopAt(0.75, QColor(178, 171, 210));
        grad.setColorStopAt(1.0, QColor(84, 39, 136));
    }

    if (invert) {
        QMap<double, QColor> currentStops = grad.colorStops();
        grad.clearColorStops();
        for (auto it = currentStops.constBegin(); it != currentStops.constEnd(); ++it) {
            grad.setColorStopAt(1.0 - it.key(), it.value());
        }
    }
    return grad;
}

QWidget* createSeismicView(
    const std::vector<std::vector<float>>& data,
    const QString& title,
    QWidget* parent,
    float dt)
{
    if (data.empty() || data[0].empty()) {
        qDebug() << "Empty seismic data";
        return nullptr;
    }

    int nx = static_cast<int>(data.size());        // 总道数 Traces
    int nz = static_cast<int>(data[0].size());     // 每道采样点数 Samples

    // 判断是按真实时间还是按样点显示 (dt 在常见地震采样范围内视为时间模式)
    bool isTimeAxis = (dt > 0.0f && dt < 0.2f);
    double yMax = isTimeAxis ? ((nz - 1) * dt) : (nz - 1);
    double yStep = isTimeAxis ? dt : 1.0;

    // 1. 创建容器
    QWidget* widget = new QWidget(parent);
    if (!parent) {
        widget->setAttribute(Qt::WA_DeleteOnClose);
        widget->setWindowTitle(title);
        widget->resize(800, 700);
    }

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 2. 初始化 QCustomPlot
    QCustomPlot* plot = new QCustomPlot(widget);
    layout->addWidget(plot, 1);
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // 3. 图层 1: ColorMap (变密度)
    QCPColorMap* colorMap = new QCPColorMap(plot->xAxis, plot->yAxis);
    colorMap->data()->setSize(nx, nz);

    // 【核心修复 1】：将 X 轴范围设为 [-0.5, nx - 0.5]，彻底消除与 Wiggle 波形线的半道错位
    colorMap->data()->setRange(QCPRange(-0.5, nx - 0.5), QCPRange(0.0, yMax));
    
    // 填充数据并计算极值
    double vmin = 1e30, vmax = -1e30;
    for (int x = 0; x < nx; ++x) {
        for (int z = 0; z < nz; ++z) {
            double v = data[x][z];
            if (std::isfinite(v)) {
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
            else {
                v = 0.0;
            }
            colorMap->data()->setCell(x, z, v);
        }
    }

    // 初始对称色标范围
    double maxAmp = std::max(std::abs(vmin), std::abs(vmax));
    if (maxAmp < 1e-9) maxAmp = 1.0;
    colorMap->setDataRange(QCPRange(-maxAmp * 0.5, maxAmp * 0.5));
    colorMap->setInterpolate(true);

    // 4. 图层 2: Wiggle (波形图)
    auto dataPtr = std::make_shared<std::vector<std::vector<float>>>(data);
    QCPSeismicWiggle* wiggle = new QCPSeismicWiggle(plot->xAxis, plot->yAxis);
    wiggle->setData(dataPtr, static_cast<float>(yStep));
    wiggle->setPen(QPen(Qt::black, 1));
    wiggle->setVisible(true); // 默认不叠合波形，纯变密度

    // 5. 色标条设置
    QCPColorScale* colorScale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, colorScale);
    colorScale->setType(QCPAxis::atRight);
    colorScale->axis()->setLabel(QStringLiteral("振幅 (Amplitude)"));
    colorMap->setColorScale(colorScale);

    // 6. 坐标轴与地学样式
    plot->yAxis->setRangeReversed(true); // 深度/时间向下递增
    plot->xAxis->setLabel(QStringLiteral("道号 (Trace No.)"));
    plot->yAxis->setLabel(isTimeAxis ? QStringLiteral("时间 Time (s)") : QStringLiteral("采样点 (Sample Index)"));

    QFont tickFont("Segoe UI", 9);
    QFont labelFont("Segoe UI", 10, QFont::Bold);
    plot->xAxis->setTickLabelFont(tickFont); plot->xAxis->setLabelFont(labelFont);
    plot->yAxis->setTickLabelFont(tickFont); plot->yAxis->setLabelFont(labelFont);

    plot->xAxis->setRange(-0.5, nx - 0.5);
    plot->yAxis->setRange(0.0, yMax);
    //设置颜色初始是对称显示
    colorMap->rescaleDataRange(true);
    plot->replot();
    // =========================================================
    // 7. 底部控制栏 (Bottom Bar) - 与深色科技风完美融合
    // =========================================================
    QFrame* bottomBar = new QFrame(widget);
    bottomBar->setFixedHeight(42);
    bottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
    QHBoxLayout* barLayout = new QHBoxLayout(bottomBar);
    barLayout->setContentsMargins(12, 0, 12, 0);
    barLayout->setSpacing(10);

    QLabel* statusLabel = new QLabel(QStringLiteral("就绪 (Ready)"), bottomBar);
    statusLabel->setStyleSheet("color: #38bdf8; font-family: 'Consolas', monospace; font-size: 11px; border: none;");

    QLabel* lblStyle = new QLabel(QStringLiteral("色标:"), bottomBar);
    lblStyle->setStyleSheet("color: #cbd5e1; font-weight: bold; border: none;");

    QComboBox* comboStyle = new QComboBox(bottomBar);
    comboStyle->setFixedWidth(95);
    comboStyle->addItems({ "Turbo", "Seismic", "Jet", "Terrain", "Grayscale", "Hot", "Cold", "Coolwarm", "Viridis", "Plasma", "Inferno", "PuOr" });
    comboStyle->setCurrentText("Seismic");

    QCheckBox* chkInvert = new QCheckBox(QStringLiteral("反转"), bottomBar);
    chkInvert->setStyleSheet("color: #cbd5e1; border: none;");

    QCheckBox* chkImage = new QCheckBox(QStringLiteral("底图"), bottomBar);
    chkImage->setChecked(true);
    chkImage->setStyleSheet("color: #cbd5e1; border: none;");

    // 【核心修复 2】：对比度滑块校准 (初始值 50 与初始 0.5 倍 maxAmp 精确对齐)
    QSlider* sliderContrast = new QSlider(Qt::Horizontal, bottomBar);
    sliderContrast->setRange(10, 200);
    sliderContrast->setValue(50);
    sliderContrast->setFixedWidth(80);
    sliderContrast->setToolTip(QStringLiteral("调节变密度对比度"));

    QCheckBox* chkWiggle = new QCheckBox(QStringLiteral("Wiggle"), bottomBar);
    chkWiggle->setChecked(true);
    chkWiggle->setStyleSheet("color: #cbd5e1; border: none;");

    QSlider* sliderGain = new QSlider(Qt::Horizontal, bottomBar);
    sliderGain->setRange(1, 200);
    sliderGain->setValue(50);
    sliderGain->setFixedWidth(80);
    sliderGain->setToolTip(QStringLiteral("调节波形振幅增益"));

    // 颜色更新槽
    auto updateColorMap = [=]() {
        QString type = comboStyle->currentText();
        bool inv = chkInvert->isChecked();
        QCPColorGradient grad = getSeismicPresetGradient(type, inv);
        colorMap->setGradient(grad);
        plot->replot();
        };

    QObject::connect(comboStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), updateColorMap);
    QObject::connect(chkInvert, &QCheckBox::toggled, updateColorMap);

    // 图像开关
    QObject::connect(chkImage, &QCheckBox::toggled, [=](bool on) {
        colorMap->setVisible(on);
        colorScale->setVisible(on);
        plot->replot();
        });

    // 对比度联动
    QObject::connect(sliderContrast, &QSlider::valueChanged, [=](int val) {
        float factor = (100.0f / std::max(1, val)) * 0.5f;
        colorMap->setDataRange(QCPRange(-maxAmp * factor, maxAmp * factor));
        plot->replot();
        });

    // 波形联动
    QObject::connect(chkWiggle, &QCheckBox::toggled, [=](bool on) {
        wiggle->setVisible(on);
        plot->replot();
        });

    float baseGain = wiggle->getGain();
    QObject::connect(sliderGain, &QSlider::valueChanged, [=](int val) {
        wiggle->setGain(baseGain * (val / 50.0f));
        plot->replot();
        });

    // 布局装配
    barLayout->addWidget(statusLabel);
    barLayout->addStretch();
    barLayout->addWidget(lblStyle);
    barLayout->addWidget(comboStyle);
    barLayout->addWidget(chkInvert);

    QFrame* vline1 = new QFrame; vline1->setFrameShape(QFrame::VLine); vline1->setStyleSheet("color:#334155");
    barLayout->addWidget(vline1);

    barLayout->addWidget(chkImage);
    barLayout->addWidget(sliderContrast);

    QFrame* vline2 = new QFrame; vline2->setFrameShape(QFrame::VLine); vline2->setStyleSheet("color:#334155");
    barLayout->addWidget(vline2);

    barLayout->addWidget(chkWiggle);
    barLayout->addWidget(sliderGain);

    layout->addWidget(bottomBar, 0);

    // 设置默认色谱为 Turbo
    updateColorMap();

    // =========================================================
    // 8. 鼠标动态十字追踪 (显示真实道号、样点、走时秒数)
    // =========================================================
    QObject::connect(plot, &QCustomPlot::mouseMove, widget, [=](QMouseEvent* e) {
        if (!plot->axisRect()->rect().contains(e->pos())) return;

        double x = plot->xAxis->pixelToCoord(e->pos().x());
        double y = plot->yAxis->pixelToCoord(e->pos().y());

        int xi = std::clamp(static_cast<int>(std::round(x)), 0, nx - 1);
        int zi = std::clamp(static_cast<int>(std::round(y / yStep)), 0, nz - 1);

        double v = colorMap->data()->cell(xi, zi);

        if (isTimeAxis) {
            statusLabel->setText(QString("道(Trace): %1 | 走时(Time): %2 s (样点:%3) | 振幅: %4")
                .arg(xi).arg(y, 0, 'f', 3).arg(zi).arg(v, 0, 'g', 4));
        }
        else {
            statusLabel->setText(QString("道(Trace): %1 | 样点(Z): %2 | 振幅: %3")
                .arg(xi).arg(zi).arg(v, 0, 'g', 4));
        }
        });

    // =========================================================
    // 9. 右键菜单 (Context Menu) - 【核心修复 3：安全 RAII + 值捕获】
    // =========================================================
    plot->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(plot, &QCustomPlot::customContextMenuRequested, widget, [=](QPoint pos) {
        QMenu menu(plot); // 栈上安全分配，彻底杜绝悬空引用崩溃

        menu.addAction(QStringLiteral("重置视图 (Reset View)"), [=]() {
            plot->xAxis->setRange(-0.5, nx - 0.5);
            plot->yAxis->setRange(0.0, yMax);
            plot->replot();
            });

        QMenu* subCmap = menu.addMenu(QStringLiteral("色彩映射 (Color Map)"));
        for (int i = 0; i < comboStyle->count(); ++i) {
            QString name = comboStyle->itemText(i);
            subCmap->addAction(name, [=]() { comboStyle->setCurrentIndex(i); });
        }

        QMenu* subWig = menu.addMenu(QStringLiteral("波形样式 (Wiggle Mode)"));
        subWig->addAction(QStringLiteral("仅线条 (Line Only)"), [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmWiggleOnly); plot->replot(); });
        subWig->addAction(QStringLiteral("变面积填充 (Fill Only)"), [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmVariableArea); plot->replot(); });
        subWig->addAction(QStringLiteral("线条与填充 (Both)"), [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmWiggleAndVA); plot->replot(); });

        menu.addSeparator();
        menu.addAction(QStringLiteral("自动对称色标 (Symmetric)"), [=]() {
            colorMap->rescaleDataRange(true);
            QCPRange range = colorMap->dataRange();
            double mA = std::max(std::abs(range.lower), std::abs(range.upper));
            if (mA < 1e-9) mA = 1.0;
            colorMap->setDataRange(QCPRange(-mA, mA));
            plot->replot();
            });
        menu.addSeparator();

        // 导出普通图片
        menu.addAction(QStringLiteral("保存图片 (当前窗口)..."), [=]() {
            QString f = QFileDialog::getSaveFileName(widget, QStringLiteral("保存图像"), "seismic_gather.png", "PNG (*.png)");
            if (!f.isEmpty()) plot->savePng(f, 0, 0, 1.0, 100);
            });

        // 导出出版级超清图片
        menu.addAction(QStringLiteral("保存超清图片 (300 DPI Publication)..."), [=]() {
            QString f = QFileDialog::getSaveFileName(widget, QStringLiteral("保存高清出版级图件"), "seismic_publication.png", "PNG (*.png)");
            if (f.isEmpty()) return;

            // 临时等比放大字体线宽进行 3 倍光栅化
            QFont tF = plot->xAxis->tickLabelFont();
            QFont lF = plot->xAxis->labelFont();
            QPen aP = plot->xAxis->basePen();
            QPen tP = plot->xAxis->tickPen();

            double factor = 2.5;
            auto scaleFont = [&](QFont font) { font.setPointSizeF(font.pointSizeF() * factor); return font; };

            plot->xAxis->setTickLabelFont(scaleFont(tF)); plot->yAxis->setTickLabelFont(scaleFont(tF));
            plot->xAxis->setLabelFont(scaleFont(lF));     plot->yAxis->setLabelFont(scaleFont(lF));

            QPen p = aP; p.setWidthF(p.widthF() * factor);
            plot->xAxis->setBasePen(p); plot->yAxis->setBasePen(p);
            p = tP; p.setWidthF(p.widthF() * factor);
            plot->xAxis->setTickPen(p); plot->yAxis->setTickPen(p);

            plot->replot();
            plot->savePng(f, static_cast<int>(plot->width() * factor), static_cast<int>(plot->height() * factor), 1.0, 100);

            // 恢复原始样式
            plot->xAxis->setTickLabelFont(tF); plot->yAxis->setTickLabelFont(tF);
            plot->xAxis->setLabelFont(lF);     plot->yAxis->setLabelFont(lF);
            plot->xAxis->setBasePen(aP);       plot->yAxis->setBasePen(aP);
            plot->xAxis->setTickPen(tP);       plot->yAxis->setTickPen(tP);
            plot->replot();
            });

        menu.addSeparator();

        // 导出为标准 SEGY 文件
        menu.addAction(QStringLiteral("💾 导出道集为 SEGY (*.sgy)..."), [=]() {
            QString defaultName = QString("Gather_%1Traces_%2.sgy").arg(nx).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            QString fileName = QFileDialog::getSaveFileName(widget, QStringLiteral("导出地震道集为 SEGY"), defaultName, "SEGY (*.sgy *.segy);;All (*.*)");
            if (fileName.isEmpty()) return;

            SeismicIO::writeSegyFile2D(data, fileName.toStdString(), dt);

            QMessageBox::information(widget, QStringLiteral("导出成功"),
                QStringLiteral("道集已成功导出为 SEGY！\n- 总道数: %1 道\n- 每道点数: %2 点\n- 采样率 dt: %3 ms")
                .arg(nx).arg(nz).arg(dt * 1000.0f));
            });

        menu.exec(plot->mapToGlobal(pos));
        });

    plot->replot();

    if (!parent) widget->show();
    return widget;
}