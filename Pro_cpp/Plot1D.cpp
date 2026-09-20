#include"Pro_h/Plot1D.h"
#include"Pro_h/SeismicIO.h"
#include"Pro_h/QCPSeismicWiggle.h"
#include"Pro_h/SeismicView2D.h"
#include"Pro_h/SignalProcessingUtils.h"
#include <QMenu>

using namespace Data_show;

Plot1D::Plot1D(QWidget* parent)
    : QCustomPlot(parent)
{
    // 1. 基础图形对象创建
    m_mainGraph = addGraph();
    // 2. 交互设置 (保持你原有的逻辑)
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
        QCP::iSelectAxes | QCP::iSelectPlottables | QCP::iMultiSelect);
    // 设置框选模式逻辑
    selectionRect()->setPen(QPen(Qt::black, 1, Qt::DashLine));
    selectionRect()->setBrush(QColor(0, 0, 100, 50));
    setSelectionRectMode(QCP::srmNone);

    // 3. 【核心优化】应用期刊级基础样式
    applyScientificStyle();
}
//@brief 核心样式优化函数：将图表配置为高水平期刊风格
void Plot1D::applyScientificStyle()
{
    // --- A. 全局设置 ---
    setBackground(Qt::white);             // 纯白背景
    setNotAntialiasedElements(QCP::aeNone); // 开启全图抗锯齿 (线条更平滑)
    // 允许在拖拽和缩放期间暂时关闭抗锯齿，极大提升流畅度
    setNoAntialiasingOnDrag(true);
    // 开启 OpenGL 加速(如果支持) 或 快速绘制提示
    setPlottingHint(QCP::phFastPolylines, true);
    // 某些情况下，开启这个可以避免绘制屏幕外的点
    // setPlottingHint(QCP::phImmediateRefresh, false);
    m_mainGraph->setAdaptiveSampling(true);

    // --- B. 坐标轴系统 (Box Style) ---
    // 设置坐标轴矩形内的边距，防止标签被切断
    axisRect()->setAutoMargins(QCP::msLeft | QCP::msBottom | QCP::msTop | QCP::msRight);

    // 字体设置：Arial 是期刊图表最通用的无衬线字体
    QFont labelFont("Arial", 12, QFont::Bold);
    QFont tickFont("Arial", 10);

    // 配置 X轴 和 Y轴
    QList<QCPAxis*> axes;
    axes << xAxis << yAxis << xAxis2 << yAxis2; // xAxis2/yAxis2 是顶部和右侧的轴

    for (QCPAxis* axis : axes)
    {
        axis->setLabelFont(labelFont);
        axis->setTickLabelFont(tickFont);
        axis->setPadding(5); // 标签和轴的距离
        axis->setLabelPadding(6);

        // 刻度线样式：向内还是向外？
        // 很多期刊喜欢 Tick 向内 (setTickLength(0, 5))，但也常用向外。
        // 这里保持默认向外，但加粗一点
        axis->setBasePen(QPen(Qt::black, 1.2)); // 轴线加粗
        axis->setTickPen(QPen(Qt::black, 1.2)); // 刻度线加粗
        axis->setSubTickPen(QPen(Qt::black, 1.0));

        // 使得顶部和右侧的轴显示出来，形成一个封闭的盒子 (Box Style)
        if (axis == xAxis2 || axis == yAxis2) {
            axis->setVisible(true);
            axis->setTickLabels(false); // 顶部和右侧不显示数字
        }
    }

    // 将顶部/右侧轴与主轴联动
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));

    // --- C. 网格线 (Grid) ---
    // 期刊图通常要么没有网格，要么是非常淡的网格
    xAxis->grid()->setPen(QPen(QColor(230, 230, 230), 1, Qt::SolidLine)); // 极淡灰色
    yAxis->grid()->setPen(QPen(QColor(230, 230, 230), 1, Qt::SolidLine));
    xAxis->grid()->setSubGridVisible(false);
    yAxis->grid()->setSubGridVisible(false);
    xAxis->grid()->setVisible(true); // 根据喜好，也可以设为 false
    yAxis->grid()->setVisible(true);

    // --- D. 图例设置 ---
    legend->setVisible(true);
    QFont legendFont("Arial", 9);
    legend->setFont(legendFont);
    legend->setBrush(QBrush(QColor(255, 255, 255, 200))); // 半透明白色背景
    legend->setBorderPen(QPen(Qt::black, 1)); // 黑色细边框

    // 将图例放在绘图区域内部右上角 (In-layout 往往占用空间，Overlay 更紧凑)
    // 这里使用 QCPLayoutInset 方式
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
}

// 原始 float 接口：转换为 double 后调用新接口，避免代码重复
void Plot1D::setData(const std::vector<float>& vec, const QString& title)
{
    if (vec.empty()) return;

    // 转换数据
    std::vector<double> x(vec.size()), y(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        x[i] = static_cast<double>(i);
        y[i] = static_cast<double>(vec[i]);
    }

    // 调用统一的接口
    setData(x, y, title);
}

// 统一的设置接口
void Plot1D::setData(const std::vector<double>& x, const std::vector<double>& y, const QString& title)
{
    // 1. 数据转换 Qt 容器
    QVector<double> qx(x.begin(), x.end());
    QVector<double> qy(y.begin(), y.end());

    // 2. 设置数据
    // 【新增这行保险代码】：防止外部意外清除 Graph 导致野指针崩溃
    if (!m_mainGraph || graphCount() == 0) {
        m_mainGraph = addGraph();
    }
    // 2. 设置数据
    m_mainGraph->setData(qx, qy);

    // 3. 【核心优化】曲线样式
    // 颜色：使用经典的科学深蓝 (Scientific Blue)
    // 比如 Matplotlib 的默认蓝 #1f77b4，或者更深的 #004c8c
    QColor mainColor(0, 76, 140);

    m_mainGraph->setPen(QPen(mainColor, 1.5)); // 线宽 1.5 ~ 2.0 最佳

    // 填充：很多频谱图如果加上半透明填充，视觉效果会好很多
    // 如果是波形图(有正负)，通常不填充；如果是频谱(只有正)，建议填充。
    // 只有当点数较少时才开启填充，否则 20k 个点的半透明填充会非常卡
    if (title.contains("Spectrum", Qt::CaseInsensitive) && x.size() < 2000) {
        QColor brushColor = mainColor;
        brushColor.setAlpha(40);
        m_mainGraph->setBrush(QBrush(brushColor));
    }
    else {
        m_mainGraph->setBrush(Qt::NoBrush); // 点多时不填充，只画线，速度快10倍
    }

    // 散点：如果点很少，显示散点，方便看数据点
    if (x.size() < 50) {
        m_mainGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, mainColor, Qt::white, 5));
    }
    else {
        m_mainGraph->setScatterStyle(QCPScatterStyle::ssNone);
    }

    // 选中样式
    m_mainGraph->setSelectable(QCP::stDataRange);
    auto* dec = new QCPSelectionDecorator();
    // 选中时变成醒目的橙红色，形成互补色
    dec->setPen(QPen(QColor(255, 69, 0), 2));
    dec->setBrush(QBrush(QColor(255, 69, 0, 50)));
    m_mainGraph->setSelectionDecorator(dec);

    // 4. 标题设置 (优化字体)
    if (plotLayout()->rowCount() == 1) {
        plotLayout()->insertRow(0);
        // 标题通常用 Arial, 加粗, 14pt
        plotLayout()->addElement(0, 0,
            new QCPTextElement(this, title, QFont("Arial", 14, QFont::Bold)));
    }
    else {
        auto* text = qobject_cast<QCPTextElement*>(plotLayout()->element(0, 0));
        if (text) {
            text->setText(title);
            text->setFont(QFont("Arial", 14, QFont::Bold));
        }
    }

    // 修改后的代码：
    if (title.contains("Spectrum", Qt::CaseInsensitive) ||
        title.contains("PSD", Qt::CaseInsensitive))  // <--- 加上这一句
    {
        this->xAxis->setLabel("Frequency (Hz)");

        // PSD 的单位通常是 V²/Hz 或者 dB/Hz
        if (title.contains("PSD", Qt::CaseInsensitive))
            this->yAxis->setLabel("PSD (V²/Hz)");
        else
            this->yAxis->setLabel("Amplitude");
    }
    else
    {
        this->xAxis->setLabel("Time (s) / Samples");
        this->yAxis->setLabel("Amplitude");
    }

    // 图例名称
    m_mainGraph->setName("Signal Data");

    // 6. 自动缩放与重绘
    m_mainGraph->rescaleAxes();

    // 稍微留一点边距，不要让曲线顶到边框
    double yRange = yAxis->range().size();
    yAxis->setRange(yAxis->range().lower - yRange * 0.05, yAxis->range().upper + yRange * 0.1);

    this->replot();
}
// Plot1D.cpp
void Plot1D::updateDataOnly(const std::vector<double>& x, const std::vector<double>& y)
{
    // 快速转换
    QVector<double> qx(x.begin(), x.end());
    QVector<double> qy(y.begin(), y.end());

    // 只设置数据
    m_mainGraph->setData(qx, qy, true); // true 表示已经排序，稍微快一点

    // 仅仅重绘
    // 注意：不要调用 rescaleAxes()，保持 Y 轴锁定在 -1 到 1
    // 使用 replot(QCustomPlot::rpQueuedReplot) 可以避免过于频繁的刷新阻塞主线程
    this->replot(QCustomPlot::rpQueuedReplot);
}

void Plot1D::m_setName(const QString& name)
{
    // 如果还没有标题元素，则创建一个
    if (!m_titleElement) {
        // 在布局的第一行插入一行
        plotLayout()->insertRow(0);
        // 创建标题元素
        m_titleElement = new QCPTextElement(this, name, QFont("sans", 12, QFont::Bold));
        m_titleElement->setTextColor(Qt::black);
        // 添加到布局 (0行, 0列)
        plotLayout()->addElement(0, 0, m_titleElement);
    }
    else {
        // 如果已有，直接更新文本
        m_titleElement->setText(name);
    }
}

void Plot1D::setSampleRate(double fs) {
    if (fs > 0) m_sampleRate = fs;
}
double Plot1D::sampleRate() const {
    return m_sampleRate;
}

void Plot1D::mousePressEvent(QMouseEvent* event)
{
    if (xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeDrag(xAxis->orientation());
    else if (yAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeDrag(yAxis->orientation());
    else if (QApplication::keyboardModifiers() & Qt::ControlModifier)
        setSelectionRectMode(QCP::srmZoom);
    else if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
        setSelectionRectMode(QCP::srmSelect);
    else
    {
        axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
        setSelectionRectMode(QCP::srmNone);
    }

    QCustomPlot::mousePressEvent(event);
}
void Plot1D::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier)
        axisRect()->setRangeZoom(xAxis->orientation());
    else if (event->modifiers() & Qt::ShiftModifier)
        axisRect()->setRangeZoom(yAxis->orientation());
    else if (xAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeZoom(xAxis->orientation());
    else if (yAxis->selectedParts().testFlag(QCPAxis::spAxis))
        axisRect()->setRangeZoom(yAxis->orientation());
    else
        axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    QCustomPlot::wheelEvent(event);
}

void Plot1D::contextMenuEvent(QContextMenuEvent* event)
{
    QPoint pos = event->pos();
    QCustomPlot* targetPlot = this;

    QMenu menu;

    // -------------------------------
    // 图例菜单
    // -------------------------------
    if (targetPlot->legend->selectTest(pos, false) >= 0)
    {
        menu.addAction("Top Left", [=]() {
            targetPlot->axisRect()->insetLayout()
                ->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
            targetPlot->replot();
            });
        menu.addAction("Top Right", [=]() {
            targetPlot->axisRect()->insetLayout()
                ->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
            targetPlot->replot();
            });
    }
    else
    {
        menu.addAction("Reset View", [=]() {
            targetPlot->rescaleAxes();
            targetPlot->replot();
            });

        if (targetPlot->selectedGraphs().size() > 0)
        {
            menu.addAction("Remove Selected Graph", [=]() {
                if (!targetPlot->selectedGraphs().isEmpty()) {
                    targetPlot->removeGraph(targetPlot->selectedGraphs().first());
                    targetPlot->replot();
                }
                });
        }
    }
    QAction* actSetFs = menu.addAction(QString("Set Sample Rate (Current: %1 Hz)").arg(m_sampleRate));

    connect(actSetFs, &QAction::triggered, [this]() {
        bool ok;
        double newFs = QInputDialog::getDouble(
            this, // parent 设为 this (即 Plot1D 对象)
            "Set Sample Rate",
            "Enter the sample rate (Hz) for spectral analysis:",
            this->m_sampleRate, // 【修改2】当前值从成员变量读取
            0.01,
            1000000.0,
            2,
            &ok
        );

        if (ok) {
            // 更新共享的采样率变量
            this->setSampleRate(newFs); // 调用 setter
            // 可以在状态栏或 ToolTip 显示一下
            QToolTip::showText(QCursor::pos(), QString("Sample rate set to %1 Hz.").arg(m_sampleRate));
        }
        });
    // ==========================================================
    // 关键修正：判断当前显示的是否已经是频谱 (Spectrum)
    // ==========================================================
    bool isSpectrum = false;

    // 方法1：检查 Y 轴标签 (推荐，因为你在 setData 时设置了 Label)
    if (targetPlot->yAxis->label().contains("Spectrum", Qt::CaseInsensitive) ||
        targetPlot->xAxis->label().contains("Frequency", Qt::CaseInsensitive))
    {
        isSpectrum = true;
    }
    // -------------------------------
    // 检查是否有图层和选中
    // -------------------------------
    bool hasGraph = (targetPlot->graphCount() > 0);
    bool hasSelection = hasGraph && !targetPlot->graph(0)->selection().isEmpty();

    if (hasGraph) // 只有存在 Graph 时才显示这些高级功能
    {
        // =========================================================
        // 定义一个 Helper Lambda 用于提取数据
        // 这样 S变换、STFT、FFT 都可以复用这一段逻辑
        // =========================================================
        auto extractSelectedData = [=](std::vector<double>& outT, std::vector<double>& outSig, double& outDt) -> bool
            {
                QCPGraph* m_mainGraph = targetPlot->graph(0);
                if (!m_mainGraph) return false;

                auto dataMap = m_mainGraph->data();
                QList<QCPDataRange> ranges;

                // 策略：如果有选中则取选中，否则取全部
                if (!m_mainGraph->selection().isEmpty())
                    ranges = m_mainGraph->selection().dataRanges();
                else
                    ranges << QCPDataRange(0, dataMap->size()); // 全部范围

                if (ranges.isEmpty()) return false;

                // 1. 提取原始数据
                for (const QCPDataRange& range : ranges) {
                    auto itBegin = dataMap->begin() + range.begin();
                    auto itEnd = dataMap->begin() + range.end();
                    if (range.end() > dataMap->size()) itEnd = dataMap->end();
                    for (auto it = itBegin; it != itEnd; ++it) {
                        outT.push_back(it->key);
                        outSig.push_back(it->value);
                    }
                }

                // 2. 长度检查
                if (outSig.size() > 20000) { // 放宽一点限制
                    outSig.resize(20000);
                    outT.resize(20000);
                    QToolTip::showText(QCursor::pos(), "Data > 20k. Truncated.");
                }
                else if (outSig.size() < 10) {
                    QToolTip::showText(QCursor::pos(), "Data too short!");
                    return false;
                }

                // 3. 智能计算采样率 (dt)
                // 优先使用写死的 0.001 (为了安全)，但如果是全部数据且为索引，则尝试转换
                // TODO: 最好的方法是 Plot1D 成员变量存储真实的 fs
                outDt = 1/m_sampleRate;

                // 如果图表已经是物理时间（秒），可以尝试反算：
                // if (outT.size() > 1) {
                //    double calc_dt = (outT.back() - outT.front()) / (outT.size() - 1);
                //    if (calc_dt < 0.9) outDt = calc_dt; // 如果间隔明显小于1，认为是秒
                // }

                // 4. 将 X 轴转换为秒 (如果它原本是索引)
                // 如果 outT[1] - outT[0] == 1.0 (索引)，则乘 dt
                if (outT.size() > 1 && std::abs(outT[1] - outT[0] - 1.0) < 1e-6) {
                    for (size_t i = 0; i < outT.size(); ++i) outT[i] *= outDt;
                }

                return true;
            };

// -------------------------------
// 功能 1: S-Transform 功能 2:STFT
// -------------------------------
        if (hasSelection && !isSpectrum)
        {
            QAction* actST = menu.addAction("S-Transform on Selection");
            connect(actST, &QAction::triggered, [this, extractSelectedData]() {
                std::vector<double> t, sig;
                double dt = 1/ this->sampleRate();
                if (!extractSelectedData(t, sig, dt)) return;

                double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量
                double f_min = 0.5;
                double f_max = std::min(fs / 2.0, 50.0); // 关注 0-50Hz
                double alpha = (f_max - f_min) / 100.0; if (alpha <= 0) alpha = 0.5;

                auto st_result = st_transform(t, sig, f_min, f_max, alpha);
                // ... [显示代码保持不变] ...
                // 注意：createSeismicView 需确保 floatData 转置正确
                std::vector<std::vector<float>> floatData = complexToFloat(st_result);
                QWidget* wid = createSeismicView(SeismicUtils::transposeMatrix(floatData), "S-Transform", nullptr);

                // 设置 Range
                QCustomPlot* plot = wid->findChild<QCustomPlot*>();
                if (plot && plot->plottable(0)) {
                    QCPColorMap* map = qobject_cast<QCPColorMap*>(plot->plottable(0));
                    map->data()->setRange(QCPRange(t.front(), t.back()), QCPRange(f_min, f_max));
                    plot->xAxis->setRange(t.front(), t.back());
                    plot->yAxis->setRange(f_min, f_max);
                    plot->xAxis->setLabel("Time (s)"); plot->yAxis->setLabel("Freq (Hz)");
                    plot->yAxis->setRangeReversed(false);
                    plot->replot();
                }
                wid->show();
                });

            QAction* actSTFT = menu.addAction("STFT on Selection");
            connect(actSTFT, &QAction::triggered, [this, extractSelectedData]() {
                std::vector<double> t, sig;
                double dt = 1 / this->sampleRate();
                if (!extractSelectedData(t, sig, dt)) return;

                double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量
                // STFT 参数
                int winSize = 256; int hop = 32; int nfft = 2048;

                auto spec = stft_fft(sig, fs, winSize, hop, nfft);

                // ... [显示代码保持不变] ...
                std::vector<std::vector<float>> floatData = doubleToFloat2D(spec);
                auto transposed = SeismicUtils::transposeMatrix(floatData);
                QWidget* wid = createSeismicView(transposed, "STFT", nullptr);

                QCustomPlot* plot = wid->findChild<QCustomPlot*>();
                if (plot && plot->plottable(0)) {
                    QCPColorMap* map = qobject_cast<QCPColorMap*>(plot->plottable(0));
                    // STFT 必须设置全范围 0 ~ fs/2
                    map->data()->setRange(QCPRange(t.front(), t.back()), QCPRange(0, fs / 2.0));
                    // View 只看 0 ~ 50Hz
                    plot->xAxis->setRange(t.front(), t.back());
                    plot->yAxis->setRange(0.5, 50.0);
                    plot->xAxis->setLabel("Time (s)"); plot->yAxis->setLabel("Freq (Hz)");
                    plot->yAxis->setRangeReversed(false);
                    plot->replot();
                }
                wid->show();
                });

        }
// -------------------------------
// 功能 3: CWT (Morlet Wavelet)
// -------------------------------
        if (hasSelection && !isSpectrum)
        {
            QAction* actCWT = menu.addAction("Time-Frequency (CWT)");

            connect(actCWT, &QAction::triggered, [this, extractSelectedData,targetPlot]()
                {
                    std::vector<double> t_raw, sig_raw;
                    double dt = 1 / this->sampleRate();

                    // 1. 提取原始数据
                    if (!extractSelectedData(t_raw, sig_raw, dt)) return;

                    double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量

                    // ==========================================================
                    // 关键修正：数据量过大时的自动降采样保护
                    // ==========================================================
                    std::vector<double> sig_proc;
                    std::vector<double> t_proc;
                    double dt_proc = dt;

                    int nRaw = sig_raw.size();
                    int limit = 10000; // 限制最大处理点数为 1万点 (保证秒出且不崩)

                    if (nRaw > limit)
                    {
                        // 计算降采样步长
                        int step = std::ceil((double)nRaw / limit);

                        // 提示用户 (可选)
                        QToolTip::showText(QCursor::pos(),
                            QString("Data too large (%1 pts). Downsampling by %2...").arg(nRaw).arg(step));

                        sig_proc.reserve(limit + 100);
                        t_proc.reserve(limit + 100);

                        // 简单的抽取 (Decimation)
                        // 更好的做法是先低通滤波再抽取，防止混叠，但为了速度这里直接抽取
                        for (int i = 0; i < nRaw; i += step) {
                            sig_proc.push_back(sig_raw[i]);
                            t_proc.push_back(t_raw[i]);
                        }

                        // 更新新的采样率
                        dt_proc = dt * step;
                    }
                    else
                    {
                        // 数据量正常，直接拷贝
                        sig_proc = sig_raw;
                        t_proc = t_raw;
                    }
                    // ==========================================================

                    // 2. CWT 参数
                    double f_min = 0.5;
                    // 最大频率不能超过新的 Nyquist 频率
                    double f_max = std::min(fs / 2.0, 60.0);
                    int num_freqs = 150;

                    // 3. 执行安全版计算
                    QApplication::setOverrideCursor(Qt::WaitCursor);

                    // 调用上面修改过的 cwt_morlet_safe
                    auto cwt_complex = cwt_morlet_safe(sig_proc, fs, f_min, f_max, num_freqs,6.0);

                    QApplication::restoreOverrideCursor();

                    // 4. 检查是否计算失败
                    if (cwt_complex.empty()) {
                        QMessageBox::critical(targetPlot, "Error",
                            "CWT failed due to insufficient memory.\nPlease select a shorter range.");
                        return;
                    }

                    // 5. 显示逻辑 (保持不变，但使用 t_proc)
                    std::vector<std::vector<float>> floatData = complexToFloat(cwt_complex);
                    auto transposedData = SeismicUtils::transposeMatrix(floatData);

                    std::string title = "CWT Spectrum (Morlet)";
                    QWidget* wid = createSeismicView(transposedData, title.c_str(), nullptr);

                    QCustomPlot* plot = wid->findChild<QCustomPlot*>();
                    if (plot && plot->plottable(0))
                    {
                        QCPColorMap* m_colorMap = qobject_cast<QCPColorMap*>(plot->plottable(0));

                        double t_start = t_proc.front();
                        double t_end = t_proc.back();

                        m_colorMap->data()->setRange(QCPRange(t_start, t_end), QCPRange(f_min, f_max));
                        plot->xAxis->setRange(t_start, t_end);
                        plot->yAxis->setRange(f_min, f_max);

                        plot->xAxis->setLabel("Time (s)");
                        plot->yAxis->setLabel("Frequency (Hz)");
                        plot->yAxis->setRangeReversed(false);
                        plot->replot();
                    }
                    wid->show();
                });
        }

// ---------------------------------------------------------
// 功能: VMD 分解
// ---------------------------------------------------------
        if (!isSpectrum && hasSelection)
        {
            QAction* actVMD = menu.addAction("Variational Mode Decomposition (VMD)");
            connect(actVMD, &QAction::triggered, [this, extractSelectedData,targetPlot]()
                {
                    // 1. 获取数据
                    std::vector<double> t, sig;
                    double dt = 1 / this->sampleRate();
                    if (!extractSelectedData(t, sig, dt)) return;

                    double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量

                    // ---------------------------------------------------------
                    // 2. 弹出高级参数设置对话框
                    // ---------------------------------------------------------
                    QDialog dlg(targetPlot);
                    dlg.setWindowTitle("VMD Algorithm Settings");
                    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
                    dlg.resize(450, 400); // 稍微设大一点，以便显示说明文字

                    QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
                    mainLayout->setSpacing(15); // 增加间距，不那么拥挤

                    // --- A. 输入区域 (FormLayout) ---
                    QGroupBox* inputGroup = new QGroupBox("Parameters", &dlg);
                    QFormLayout* formLayout = new QFormLayout(inputGroup);
                    formLayout->setLabelAlignment(Qt::AlignLeft); // 标签左对齐

                    // 参数 K
                    QSpinBox* spinK = new QSpinBox(&dlg);
                    spinK->setRange(2, 16);
                    spinK->setValue(4);
                    spinK->setToolTip("Determines how many modes (IMFs) to extract.");
                    formLayout->addRow("Number of Modes (K):", spinK);

                    // 参数 Alpha
                    QDoubleSpinBox* spinAlpha = new QDoubleSpinBox(&dlg);
                    spinAlpha->setRange(100.0, 100000.0);
                    spinAlpha->setValue(2000.0);
                    spinAlpha->setSingleStep(500.0);
                    spinAlpha->setDecimals(0);
                    spinAlpha->setGroupSeparatorShown(true); // 显示千分位 (2,000)
                    spinAlpha->setToolTip("Controls the bandwidth of the modes. Higher = Narrower.");
                    formLayout->addRow("Bandwidth Constraint (Alpha):", spinAlpha);

                    mainLayout->addWidget(inputGroup);

                    // --- B. 详细说明区域 (Info Box) ---
                    QGroupBox* helpGroup = new QGroupBox("Parameter Guide", &dlg);
                    QVBoxLayout* helpLayout = new QVBoxLayout(helpGroup);

                    QLabel* labelHelp = new QLabel(&dlg);
                    labelHelp->setWordWrap(true); // 允许自动换行
                    labelHelp->setStyleSheet("QLabel { color: #333; font-size: 11px; }"); // 字体稍微改小，颜色深灰

                    // 使用 HTML 格式化文本，使其具有可读性
                    labelHelp->setText(
                        "<b>1. Number of Modes (K):</b><br>"
                        "Controls the decomposition level.<br>"
                        "<font color='gray'>&nbsp;&nbsp;• <b>Seismic Data:</b> Recommended 3 ~ 6.<br>"
                        "&nbsp;&nbsp;• <b>Machinery Faults:</b> Recommended 4 ~ 8.<br>"
                        "&nbsp;&nbsp;• Note: Too large K may cause mode splitting.</font><br><br>"

                        "<b>2. Bandwidth Constraint (Alpha):</b><br>"
                        "Controls the 'tightness' of the band-pass filters.<br>"
                        "<font color='gray'>&nbsp;&nbsp;• <b>Low (500~1000):</b> Wide bandwidth. Good for impulsive/transient signals.<br>"
                        "&nbsp;&nbsp;• <b>Moderate (2000):</b> Standard setting. Good balance.<br>"
                        "&nbsp;&nbsp;• <b>High (>3000):</b> Narrow bandwidth. Good for separating close frequencies.</font>"
                    );

                    helpLayout->addWidget(labelHelp);
                    mainLayout->addWidget(helpGroup);

                    // --- C. 底部按钮 ---
                    QDialogButtonBox* buttonBox = new QDialogButtonBox(
                        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                    mainLayout->addWidget(buttonBox);

                    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

                    // --- 显示并获取结果 ---
                    if (dlg.exec() != QDialog::Accepted) return;

                    int K = spinK->value();
                    double alpha = spinAlpha->value();

                    // 3. 执行计算 (耗时操作，显示漏斗)
                    QApplication::setOverrideCursor(Qt::WaitCursor);

                    // 调用算法 (请确保 compute_vmd 可见)
                    VmdResult res = compute_vmd(sig, fs, K, alpha, 0.0, 1e-7);

                    QApplication::restoreOverrideCursor();

        // ---------------------------------------------------------
        // 4. 可视化 (使用 QScrollArea 防止窗口过大)
        // ---------------------------------------------------------

        // A. 创建主窗口容器
                    QWidget* resWindow = new QWidget();
                    resWindow->setAttribute(Qt::WA_DeleteOnClose);
                    resWindow->setWindowTitle(QString("VMD Results (K=%1, Alpha=%2)").arg(K).arg(alpha));
                    resWindow->resize(900, 800); // 给一个合理的初始大小

                    // B. 创建主布局 (用于放 ScrollArea)
                    QVBoxLayout* mainLayout2 = new QVBoxLayout(resWindow);
                    mainLayout2->setContentsMargins(0, 0, 0, 0); // 去掉边距，让滚动条贴边

                    // C. 创建滚动区域
                    QScrollArea* scrollArea = new QScrollArea(resWindow);
                    scrollArea->setWidgetResizable(true); // 关键：让内部控件宽度自适应窗口宽度
                    mainLayout2->addWidget(scrollArea);

                    // D. 创建滚动区域内部的长容器 (Content Widget)
                    QWidget* scrollContent = new QWidget();
                    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
                    contentLayout->setSpacing(10); // 图与图之间的间距

 
                    // E1. 绘制原始信号
                    {
                        Data_show::Plot1D* pOrg = new Data_show::Plot1D(scrollContent);
                        // 设定最小高度，确保每个图都有足够的空间显示，迫使滚动条出现
                        pOrg->setMinimumHeight(250);
                        pOrg->setData(t, sig, "Original Signal");
                        contentLayout->addWidget(pOrg);
                    }

                    // E2. 绘制分解出的 IMF
                    for (int k = 0; k < K; ++k)
                    {
                        Data_show::Plot1D* pMode = new Data_show::Plot1D(scrollContent);
                        pMode->setMinimumHeight(200); // IMF 子图高度设为 200

                        // 标题显示中心频率
                        QString title = QString("IMF %1 (Center Freq: %2 Hz)")
                            .arg(k + 1)
                            .arg(QString::number(res.center_freqs[k], 'f', 2));

                        pMode->setData(t, res.modes[k], title);

                        // 样式微调：使用不同颜色区分
                        QColor col;
                        col.setHsv((k * 360 / K) % 360, 200, 180);
                        pMode->graph(0)->setPen(QPen(col, 1.5));
                        pMode->graph(0)->setBrush(Qt::NoBrush);

                        contentLayout->addWidget(pMode);
                    }

                    // F. 收尾工作
                    // 在最底部加一个弹簧，防止图表少的时候被拉伸变形
                    contentLayout->addStretch();

                    // 将长容器设置给 ScrollArea
                    scrollArea->setWidget(scrollContent);

                    resWindow->show();

                });
        }
// -------------------------------
// 功能 4: Welch PSD (新增)
// -------------------------------
        if (!isSpectrum) // 只有在时域信号上才显示
        {
            QAction* actWelch = menu.addAction(hasSelection ? "Show PSD (Welch Method)" : "Show PSD (Welch All)");

            connect(actWelch, &QAction::triggered, [this, extractSelectedData]()
                {
                    std::vector<double> t, sig;
                    double dt = 1 / this->sampleRate();
                    if (!extractSelectedData(t, sig, dt)) return;

                    double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量

                    // 2. Welch 参数设置
                    // 策略：窗口长度通常取数据长度的 1/8 到 1/4，或者固定为 1024/2048
                    // 这样既能保证频率分辨率，又能有足够的段数来做平均
                    int nData = sig.size();
                    int nperseg = 1024;

                    // 如果数据太短，就缩短窗口；如果数据很长，适当增加窗口
                    if (nData < 1024) nperseg = nData / 2;
                    else if (nData > 20000) nperseg = 4096;

                    // 保证是 2 的幂次，FFT 会更快 (可选)
                    // nperseg = std::pow(2, std::floor(std::log2(nperseg)));

                    std::vector<double> freqs, psd;

                    // 3. 执行计算
                    compute_welch_psd(sig, fs, nperseg, freqs, psd);

                    // 4. 显示结果
                    QWidget* w = new QWidget();
                    w->setAttribute(Qt::WA_DeleteOnClose);
                    w->setWindowTitle("Power Spectral Density (Welch)");
                    w->resize(800, 500);

                    QVBoxLayout* l = new QVBoxLayout(w);
                    Data_show::Plot1D* p = new Data_show::Plot1D(w);
                    l->addWidget(p);

                    QString info = QString("PSD (Welch): Win=%1, Overlap=50%").arg(nperseg);
                    p->setData(freqs, psd, info);

                    // 5. 设置为对数坐标 (PSD 通常在对数坐标下看)
                    // 震动/地震数据动态范围极大，线性坐标往往看不清
                    p->yAxis->setScaleType(QCPAxis::stLogarithmic);

                    // 设置一下合适的范围
                    p->xAxis->setRange(0, std::min(fs / 2.0, 100.0)); // 默认看前 100Hz
                    QCPRange validRange = p->yAxis->range();
                    // 避免 log(0) 问题，稍微调整下限
                    if (validRange.lower <= 0) p->yAxis->setRange(1e-9, validRange.upper);

                    p->yAxis->setLabel("PSD (V²/Hz)"); // 单位

                    w->show();
                });
        }
// -------------------------------
// 功能 5: 包络分析 (Envelope Analysis)
// -------------------------------
        if (!isSpectrum) // 仅限时域信号
        {
            QAction* actEnvelope = menu.addAction(hasSelection ? "Show Envelope (Hilbert)" : "Show Envelope (All)");

            connect(actEnvelope, &QAction::triggered, [=]()
                {
                    std::vector<double> t, sig;
                    double dt = 0.001;

                    // 1. 提取数据
                    if (!extractSelectedData(t, sig, dt)) return;

                    // 2. 计算包络
                    std::vector<double> env = compute_hilbert_envelope(sig);

                    // 3. 创建显示窗口
                    QWidget* w = new QWidget();
                    w->setAttribute(Qt::WA_DeleteOnClose);
                    w->setWindowTitle("Hilbert Envelope Analysis");
                    w->resize(800, 400);

                    QVBoxLayout* l = new QVBoxLayout(w);
                    Data_show::Plot1D* p = new Data_show::Plot1D(w);
                    l->addWidget(p);

                    // -------------------------------------------------
                    // 核心绘图逻辑：叠加显示
                    // -------------------------------------------------

                    // 第一层：绘制原始信号 (作为背景)
                    p->setData(t, sig, "Envelope Analysis"); // 这会设置 Graph(0)
                    QCPGraph* graphSig = p->graph(0);

                    // 设置原信号为浅蓝色或灰色，显得不喧宾夺主
                    graphSig->setPen(QPen(QColor(180, 180, 180), 1));
                    graphSig->setBrush(Qt::NoBrush); // 原信号不填充
                    graphSig->setName("Original Signal");

                    // 第二层：绘制包络线 (作为前景)
                    QCPGraph* graphEnv = p->addGraph(); // 添加新图层

                    // 转换数据格式
                    QVector<double> qt(t.begin(), t.end());
                    QVector<double> qenv(env.begin(), env.end());
                    graphEnv->setData(qt, qenv);

                    // 设置包络线为醒目的红色，加粗
                    graphEnv->setPen(QPen(Qt::red, 2));
                    graphEnv->setName("Hilbert Envelope");

                    // -------------------------------------------------
                    // 优化视觉效果
                    // -------------------------------------------------

                    // 1. 让 X 轴和 Y 轴适应两个图形的范围
                    p->rescaleAxes();

                    // 2. 重新应用一下期刊样式 (确保新加的 Graph 也是抗锯齿的)
                    // 注意：由于我们在外部手动修改了 pen，这里不需要再次完全 applyScientificStyle
                    // 但需要确保图例可见
                    p->legend->setVisible(true);

                    w->show();
                });
        }
        // -------------------------------
        // 功能 4: FFT Spectrum
        // -------------------------------

        QAction* actFFT = menu.addAction(hasSelection ? "Show Amplitude Spectrum (Selection)" : "Show Amplitude Spectrum (All)");
        connect(actFFT, &QAction::triggered, [this, extractSelectedData]() {
            std::vector<double> t, sig;
            double dt = 1 / this->sampleRate();
            if (!extractSelectedData(t, sig, dt)) return;

            double fs = this->sampleRate(); // 调用 getter // <--- 使用我们共享的变量
            int N = sig.size();

            // 去直流
            double mean = std::accumulate(sig.begin(), sig.end(), 0.0) / N;
            for (double& v : sig) v -= mean;

            Eigen::FFT<double> fft;
            std::vector<std::complex<double>> fftOut;
            fft.fwd(fftOut, sig);

            int nFreq = N / 2 + 1;
            std::vector<double> freqs, mags;
            freqs.reserve(nFreq); mags.reserve(nFreq);

            for (int i = 0; i < nFreq; ++i) {
                double f = i * fs / N;
                double mag = std::abs(fftOut[i]);
                mag = (i == 0 || i == nFreq - 1) ? mag / N : mag * 2.0 / N;
                freqs.push_back(f);
                mags.push_back(mag);
            }

            QWidget* w = new QWidget();
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->setWindowTitle("Spectrum");
            w->resize(800, 400);
            QVBoxLayout* l = new QVBoxLayout(w);
            Data_show::Plot1D* p = new Data_show::Plot1D(w);
            l->addWidget(p);

            p->setData(freqs, mags, "Amplitude Spectrum");
            p->xAxis->setRange(0, 50); // 默认看 0-50Hz 对比
            w->show();
            });

        // -------------------------------
        // 底部通用菜单
        // -------------------------------
        menu.addSeparator();
        if (hasSelection) {
            menu.addAction("Clear Selection", [=]() {
                targetPlot->graph(0)->setSelection(QCPDataSelection());
                targetPlot->replot();
                });
        }
        menu.addSeparator(); // 加个分割线

        // --- 导出功能 ---
        QMenu* exportMenu = menu.addMenu("Export Plot");

        //// 1. 导出为 PDF (矢量图，发文章首选，放大不失真)
        //exportMenu->addAction("To PDF (Vector)", [=]() {
        //    QString fileName = QFileDialog::getSaveFileName(targetPlot, "Save PDF", "", "PDF Files (*.pdf)");
        //    if (!fileName.isEmpty()) {
        //        // noCosmetic: 保证线宽按比例缩放，noExportCache: 保证导出最高质量
        //        targetPlot->savePdf(fileName, 0, 0, QCP::epNoCosmetic);
        //    }
        //    });

        // 2. 导出为 PNG (位图，PPT展示用，支持超高分辨率)
        exportMenu->addAction("To PNG (High Res)", [=]() {
            QString fileName = QFileDialog::getSaveFileName(targetPlot, "Save PNG", "", "PNG Files (*.png)");
            if (!fileName.isEmpty()) {
                // 这里的 scale=2.0 代表 2倍分辨率 (比如 1920x1080 -> 3840x2160)
                // 能够让文字和线条极度清晰
                targetPlot->savePng(fileName, 0, 0, 2.0, 100);
            }
            });
        // 在 contextMenuEvent 函数的末尾，menu.exec() 之前添加：

        menu.addSeparator();

        // ---------------------------------------------------------
        // 功能: 导出数据 (CSV/TXT)
        // ---------------------------------------------------------
        QAction* actExportData = menu.addAction("Export Data to CSV/TXT");

        connect(actExportData, &QAction::triggered, [=]()
            {
                // 0. 安全检查
                if (targetPlot->graphCount() == 0) {
                    QMessageBox::warning(targetPlot, "Export Failed", "No graph data to export.");
                    return;
                }

                QCPGraph* graph = targetPlot->graph(0);
                bool hasSelection = !graph->selection().isEmpty();

                // 1. 弹出保存对话框
                QString filters = "CSV Files (*.csv);;Text Files (*.txt);;All Files (*.*)";
                QString defaultName = QString("Data_Export_%1.csv")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

                QString fileName = QFileDialog::getSaveFileName(targetPlot, "Export Data", defaultName, filters);

                if (fileName.isEmpty()) return; // 用户取消

                // 2. 准备文件写入
                QFile file(fileName);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QMessageBox::critical(targetPlot, "Export Error", "Cannot open file for writing:\n" + file.errorString());
                    return;
                }

                QTextStream out(&file);

                // 3. 确定分隔符 (CSV用逗号，TXT通常用Tab或空格)
                QString sep = ",";
                if (fileName.endsWith(".txt", Qt::CaseInsensitive)) {
                    sep = "\t"; // 使用 Tab 分隔，方便复制到 Excel/Origin
                }

                // 4. 写入表头 (Header)
                // 智能获取坐标轴名称作为表头
                QString labelX = targetPlot->xAxis->label();
                QString labelY = targetPlot->yAxis->label();
                if (labelX.isEmpty()) labelX = "X";
                if (labelY.isEmpty()) labelY = "Y";

                // 去除 label 中的非法字符（如换行符）
                labelX.replace("\n", " ");
                labelY.replace("\n", " ");

                out << labelX << sep << labelY << "\n";

                // 5. 数据遍历逻辑
                // 如果有选中，只导出选中的；否则导出全部
                auto dataMap = graph->data();
                int count = 0;

                auto writePoint = [&](double x, double y) {
                    // 使用 'g' 格式并保留高精度 (10位有效数字)，防止数据截断
                    out << QString::number(x, 'g', 12) << sep
                        << QString::number(y, 'g', 12) << "\n";
                    count++;
                    };

                if (hasSelection)
                {
                    QList<QCPDataRange> ranges = graph->selection().dataRanges();
                    for (const QCPDataRange& range : ranges)
                    {
                        auto itBegin = dataMap->begin() + range.begin();
                        auto itEnd = dataMap->begin() + range.end();
                        if (range.end() > dataMap->size()) itEnd = dataMap->end();

                        for (auto it = itBegin; it != itEnd; ++it) {
                            writePoint(it->key, it->value);
                        }
                    }
                }
                else
                {
                    // 导出全部
                    for (auto it = dataMap->begin(); it != dataMap->end(); ++it) {
                        writePoint(it->key, it->value);
                    }
                }

                file.close();

                // 6. 成功提示 (状态栏或弹窗)
                // 如果不想弹窗打扰用户，可以用 QToolTip 或者直接忽略
                QToolTip::showText(QCursor::pos(), QString("Successfully exported %1 points.").arg(count));
            });

        menu.exec(mapToGlobal(pos));
    }
};
