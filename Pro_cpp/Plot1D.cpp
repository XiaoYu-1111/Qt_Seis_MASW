#include "Pro_h/Plot1D.h"
#include "Pro_h/SeismicIO.h"
#include "Pro_h/QCPSeismicWiggle.h"
#include "Pro_h/SeismicView2D.h"
#include "Pro_h/SignalProcessingUtils.h"

#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QScrollArea>
#include <QMessageBox>
#include <QToolTip>
#include <QTextStream>
#include <QApplication>

#include <complex>
#include <numeric>
#include <cmath>
#include <omp.h>
#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>

using namespace Data_show;

Plot1D::Plot1D(QWidget* parent)
    : QCustomPlot(parent)
{
    // 1. 初始化交互
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
        QCP::iSelectAxes | QCP::iSelectPlottables | QCP::iMultiSelect);

    // 框选设置
    selectionRect()->setPen(QPen(Qt::black, 1, Qt::DashLine));
    selectionRect()->setBrush(QColor(0, 100, 200, 40));
    setSelectionRectMode(QCP::srmNone);

    // 2. 初始化图形与期刊样式
    ensureMainGraph();
    applyScientificStyle();
}

QCPGraph* Plot1D::ensureMainGraph()
{
    // 利用 hasPlottable 严谨检查图针是否存活，彻底解决 clearGraphs 导致的内存崩溃
    if (!m_mainGraph || !hasPlottable(m_mainGraph)) {
        m_mainGraph = addGraph();
        m_mainGraph->setAdaptiveSampling(true);

        // 设置选中修饰器（只创建一次，避免内存泄漏）
        auto* dec = new QCPSelectionDecorator();
        dec->setPen(QPen(QColor(255, 69, 0), 2));
        dec->setBrush(QBrush(QColor(255, 69, 0, 40)));
        m_mainGraph->setSelectionDecorator(dec);
        m_mainGraph->setSelectable(QCP::stDataRange);
    }
    return m_mainGraph;
}

void Plot1D::clearGraphs()
{
    QCustomPlot::clearGraphs();
    m_mainGraph = nullptr; // 标志位同步置空，绝不留野指针
}

void Plot1D::applyScientificStyle()
{
    setBackground(Qt::white);
    setNotAntialiasedElements(QCP::aeNone);
    setNoAntialiasingOnDrag(true);
    setPlottingHint(QCP::phFastPolylines, true);

    axisRect()->setAutoMargins(QCP::msLeft | QCP::msBottom | QCP::msTop | QCP::msRight);

    QFont labelFont("Arial", 11, QFont::Bold);
    QFont tickFont("Arial", 9);

    QList<QCPAxis*> axes = { xAxis, yAxis, xAxis2, yAxis2 };
    for (QCPAxis* axis : axes) {
        axis->setLabelFont(labelFont);
        axis->setTickLabelFont(tickFont);
        axis->setPadding(4);
        axis->setLabelPadding(5);
        axis->setBasePen(QPen(Qt::black, 1.2));
        axis->setTickPen(QPen(Qt::black, 1.2));
        axis->setSubTickPen(QPen(Qt::black, 1.0));

        if (axis == xAxis2 || axis == yAxis2) {
            axis->setVisible(true);
            axis->setTickLabels(false);
        }
    }

    // 上右双轴联动
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));

    // 淡灰网格
    xAxis->grid()->setPen(QPen(QColor(235, 235, 235), 1, Qt::SolidLine));
    yAxis->grid()->setPen(QPen(QColor(235, 235, 235), 1, Qt::SolidLine));
    xAxis->grid()->setSubGridVisible(false);
    yAxis->grid()->setSubGridVisible(false);

    // 图例设置 (内嵌右上角)
    legend->setVisible(true);
    legend->setFont(QFont("Arial", 9));
    legend->setBrush(QBrush(QColor(255, 255, 255, 210)));
    legend->setBorderPen(QPen(QColor(200, 200, 200), 1));
    axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
}

void Plot1D::setupGraphStyle(QCPGraph* graph, const QString& title, int pointCount)
{
    QColor mainColor(0, 76, 140); // 经典深海科学蓝
    graph->setPen(QPen(mainColor, 1.5));
    graph->setName(title.isEmpty() ? "Signal Data" : title);

    // 频谱且点数合适时添加渐变半透明填充
    if (title.contains("Spectrum", Qt::CaseInsensitive) && pointCount < 3000) {
        QColor brushColor = mainColor;
        brushColor.setAlpha(35);
        graph->setBrush(QBrush(brushColor));
    }
    else {
        graph->setBrush(Qt::NoBrush);
    }

    // 极少点数时显示散点标记
    if (pointCount < 60) {
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, mainColor, Qt::white, 5));
    }
    else {
        graph->setScatterStyle(QCPScatterStyle::ssNone);
    }

    // 智能轴标签
    if (title.contains("Spectrum", Qt::CaseInsensitive) || title.contains("PSD", Qt::CaseInsensitive)) {
        xAxis->setLabel("Frequency (Hz)");
        yAxis->setLabel(title.contains("PSD", Qt::CaseInsensitive) ? "PSD (V²/Hz)" : "Amplitude");
    }
    else if (!yAxis->label().contains("Depth")) {
        xAxis->setLabel("Time (s) / Samples");
        yAxis->setLabel("Amplitude");
    }
}

void Plot1D::setTitle(const QString& title)
{
    if (title.isEmpty()) return;

    if (!m_titleElement) {
        if (plotLayout()->rowCount() == 1) {
            plotLayout()->insertRow(0);
        }
        m_titleElement = new QCPTextElement(this, title, QFont("Arial", 12, QFont::Bold));
        m_titleElement->setTextColor(QColor(30, 41, 59));
        plotLayout()->addElement(0, 0, m_titleElement);
    }
    else {
        m_titleElement->setText(title);
    }
}

void Plot1D::m_setName(const QString& name)
{
    setTitle(name);
}

void Plot1D::setData(const std::vector<float>& vec, const QString& title)
{
    if (vec.empty()) return;
    QVector<double> qx(vec.size()), qy(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        qx[i] = static_cast<double>(i);
        qy[i] = static_cast<double>(vec[i]);
    }
    setData(qx, qy, title);
}

void Plot1D::setData(const std::vector<double>& x, const std::vector<double>& y, const QString& title)
{
    QVector<double> qx(x.begin(), x.end());
    QVector<double> qy(y.begin(), y.end());
    setData(qx, qy, title);
}

void Plot1D::setData(const QVector<double>& x, const QVector<double>& y, const QString& title)
{
    if (x.isEmpty() || y.isEmpty()) return;

    ensureMainGraph();
    m_mainGraph->setData(x, y);

    setupGraphStyle(m_mainGraph, title, x.size());
    setTitle(title);

    m_mainGraph->rescaleAxes();

    // 留出 8% 的上下边距，避免波峰波谷被切断
    double ySpan = yAxis->range().size();
    if (ySpan > 1e-6) {
        yAxis->setRange(yAxis->range().lower - ySpan * 0.05, yAxis->range().upper + ySpan * 0.08);
    }

    replot();
}

void Plot1D::updateDataOnly(const std::vector<double>& x, const std::vector<double>& y)
{
    QVector<double> qx(x.begin(), x.end());
    QVector<double> qy(y.begin(), y.end());
    updateDataOnly(qx, qy);
}

void Plot1D::updateDataOnly(const QVector<double>& x, const QVector<double>& y)
{
    ensureMainGraph();
    m_mainGraph->setData(x, y, true);
    replot(QCustomPlot::rpQueuedReplot); // 队列重绘，提升快速刷新流畅度
}

void Plot1D::setSampleRate(double fs)
{
    if (fs > 0.0) m_sampleRate = fs;
}

double Plot1D::sampleRate() const
{
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
    else {
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

// ==============================================================================
// 模块化右键菜单系统
// ==============================================================================
void Plot1D::contextMenuEvent(QContextMenuEvent* event)
{
    QPoint pos = event->pos();
    QMenu menu(this);

    // 1. 图例位置
    if (legend->selectTest(pos, false) >= 0) {
        menu.addAction("Top Left", [this]() {
            axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
            replot();
            });
        menu.addAction("Top Right", [this]() {
            axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
            replot();
            });
        menu.exec(mapToGlobal(pos));
        return;
    }

    // 2. 基础视图操作
    menu.addAction("Reset View", [this]() {
        rescaleAxes();
        replot();
        });

    if (!selectedGraphs().isEmpty()) {
        menu.addAction("Remove Selected Graph", [this]() {
            removeGraph(selectedGraphs().first());
            replot();
            });
    }

    menu.addAction(QString("Set Sample Rate (Current: %1 Hz)").arg(m_sampleRate),
        this, &Plot1D::handleSetSampleRate);

    menu.addSeparator();

    // 3. 高级信号分析菜单（仅针对时域波形）
    bool isSpectrum = yAxis->label().contains("Spectrum", Qt::CaseInsensitive) ||
        xAxis->label().contains("Frequency", Qt::CaseInsensitive) ||
        yAxis->label().contains("Depth", Qt::CaseInsensitive);

    bool hasSelection = (graphCount() > 0) && !graph(0)->selection().isEmpty();

    std::vector<double> t, sig;
    double dt = 0.0;
    bool hasData = extractSelectedData(t, sig, dt);

    if (hasData && !isSpectrum) {
        QMenu* tfMenu = menu.addMenu("Time-Frequency Analysis");
        if (hasSelection) {
            tfMenu->addAction("S-Transform", [this, t, sig]() { handleSTransform(t, sig); });
            tfMenu->addAction("STFT (Spectrogram)", [this, t, sig]() { handleSTFT(t, sig); });
            tfMenu->addAction("CWT (Morlet Wavelet)", [this, t, sig]() { handleCWT(t, sig); });
            tfMenu->addAction("VMD Decomposition", [this, t, sig]() { handleVMD(t, sig); });
        }
        else {
            QAction* disabledAct = tfMenu->addAction("Select range to enable (Shift+Drag)");
            disabledAct->setEnabled(false);
        }

        menu.addAction("Welch PSD (Power Spectrum)", [this, sig]() { handleWelchPSD(sig); });
        menu.addAction("Hilbert Envelope", [this, t, sig]() { handleHilbertEnvelope(t, sig); });
        menu.addAction("FFT Amplitude Spectrum", [this, sig]() { handleFFTSpectrum(sig); });
    }

    // 4. 清除选区与导出
    menu.addSeparator();
    if (graphCount() > 0 && !graph(0)->selection().isEmpty()) {
        menu.addAction("Clear Selection", [this]() {
            graph(0)->setSelection(QCPDataSelection());
            replot();
            });
    }

    QMenu* exportMenu = menu.addMenu("Export Plot Image");
    exportMenu->addAction("PNG (High-Res 2x)", [this]() {
        QString f = QFileDialog::getSaveFileName(this, "Save Image", "plot.png", "PNG (*.png)");
        if (!f.isEmpty()) savePng(f, 0, 0, 2.0, 100);
        });
    exportMenu->addAction("PDF (Vector)", [this]() {
        QString f = QFileDialog::getSaveFileName(this, "Save PDF", "plot.pdf", "PDF (*.pdf)");
        if (!f.isEmpty()) savePdf(f);
        });

    menu.addAction("Export Data to CSV/TXT...", this, &Plot1D::handleExportData);

    menu.exec(mapToGlobal(pos));
}

bool Plot1D::extractSelectedData(std::vector<double>& outT, std::vector<double>& outSig, double& outDt)
{
    if (graphCount() == 0 || !graph(0)) return false;

    QCPGraph* g = graph(0);
    auto dataMap = g->data();
    if (dataMap->isEmpty()) return false;

    QList<QCPDataRange> ranges;
    if (!g->selection().isEmpty()) {
        ranges = g->selection().dataRanges();
    }
    else {
        ranges << QCPDataRange(0, dataMap->size());
    }

    for (const QCPDataRange& range : ranges) {
        auto itBegin = dataMap->begin() + range.begin();
        auto itEnd = dataMap->begin() + range.end();
        if (range.end() > dataMap->size()) itEnd = dataMap->end();
        for (auto it = itBegin; it != itEnd; ++it) {
            outT.push_back(it->key);
            outSig.push_back(it->value);
        }
    }

    if (outSig.size() < 4) return false;

    if (outSig.size() > 20000) {
        outSig.resize(20000);
        outT.resize(20000);
        QToolTip::showText(QCursor::pos(), "Data truncated to 20k points for performance.");
    }

    outDt = (m_sampleRate > 0.0) ? (1.0 / m_sampleRate) : 0.001;

    // 索引转物理秒
    if (outT.size() > 1 && std::abs(outT[1] - outT[0] - 1.0) < 1e-5) {
        for (double& val : outT) val *= outDt;
    }

    return true;
}

void Plot1D::handleSetSampleRate()
{
    bool ok;
    double newFs = QInputDialog::getDouble(
        this, "Set Sample Rate",
        "Enter sample rate (Hz):",
        m_sampleRate, 0.01, 1e7, 2, &ok);

    if (ok) {
        setSampleRate(newFs);
        QToolTip::showText(QCursor::pos(), QString("Sample rate set to %1 Hz").arg(m_sampleRate));
    }
}

void Plot1D::handleSTransform(const std::vector<double>& t, const std::vector<double>& sig)
{
    double fs = sampleRate();
    double f_min = 0.5;
    double f_max = std::min(fs / 2.0, 60.0);
    double alpha = std::max(0.2, (f_max - f_min) / 100.0);

    auto st_result = st_transform(t, sig, f_min, f_max, alpha);
    std::vector<std::vector<float>> floatData = complexToFloat(st_result);

    QWidget* wid = createSeismicView(SeismicUtils::transposeMatrix(floatData), "S-Transform Spectrogram", nullptr);
    if (!wid) return;

    QCustomPlot* plot = wid->findChild<QCustomPlot*>();
    if (plot && plot->plottable(0)) {
        auto* map = qobject_cast<QCPColorMap*>(plot->plottable(0));
        map->data()->setRange(QCPRange(t.front(), t.back()), QCPRange(f_min, f_max));
        plot->xAxis->setRange(t.front(), t.back());
        plot->yAxis->setRange(f_min, f_max);
        plot->xAxis->setLabel("Time (s)");
        plot->yAxis->setLabel("Frequency (Hz)");
        plot->yAxis->setRangeReversed(false);
        plot->replot();
    }
    wid->show();
}

void Plot1D::handleSTFT(const std::vector<double>& t, const std::vector<double>& sig)
{
    double fs = sampleRate();
    auto spec = stft_fft(sig, fs, 256, 32, 2048);
    std::vector<std::vector<float>> floatData = doubleToFloat2D(spec);

    QWidget* wid = createSeismicView(SeismicUtils::transposeMatrix(floatData), "STFT Spectrogram", nullptr);
    if (!wid) return;

    QCustomPlot* plot = wid->findChild<QCustomPlot*>();
    if (plot && plot->plottable(0)) {
        auto* map = qobject_cast<QCPColorMap*>(plot->plottable(0));
        map->data()->setRange(QCPRange(t.front(), t.back()), QCPRange(0, fs / 2.0));
        plot->xAxis->setRange(t.front(), t.back());
        plot->yAxis->setRange(0.5, std::min(fs / 2.0, 60.0));
        plot->xAxis->setLabel("Time (s)");
        plot->yAxis->setLabel("Frequency (Hz)");
        plot->yAxis->setRangeReversed(false);
        plot->replot();
    }
    wid->show();
}

void Plot1D::handleCWT(const std::vector<double>& t, const std::vector<double>& sig)
{
    double fs = sampleRate();
    double f_min = 0.5;
    double f_max = std::min(fs / 2.0, 60.0);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto cwt_complex = cwt_morlet_safe(sig, fs, f_min, f_max, 150, 6.0);
    QApplication::restoreOverrideCursor();

    if (cwt_complex.empty()) {
        QMessageBox::warning(this, "CWT Failed", "Calculation failed due to memory limit.");
        return;
    }

    std::vector<std::vector<float>> floatData = complexToFloat(cwt_complex);
    QWidget* wid = createSeismicView(SeismicUtils::transposeMatrix(floatData), "CWT Spectrum (Morlet)", nullptr);
    if (!wid) return;

    QCustomPlot* plot = wid->findChild<QCustomPlot*>();
    if (plot && plot->plottable(0)) {
        auto* map = qobject_cast<QCPColorMap*>(plot->plottable(0));
        map->data()->setRange(QCPRange(t.front(), t.back()), QCPRange(f_min, f_max));
        plot->xAxis->setRange(t.front(), t.back());
        plot->yAxis->setRange(f_min, f_max);
        plot->xAxis->setLabel("Time (s)");
        plot->yAxis->setLabel("Frequency (Hz)");
        plot->yAxis->setRangeReversed(false);
        plot->replot();
    }
    wid->show();
}

void Plot1D::handleVMD(const std::vector<double>& t, const std::vector<double>& sig)
{
    double fs = sampleRate();

    QDialog dlg(this);
    dlg.setWindowTitle("VMD Decomposition");
    QVBoxLayout* lay = new QVBoxLayout(&dlg);

    QSpinBox* spinK = new QSpinBox(&dlg); spinK->setRange(2, 10); spinK->setValue(4);
    QDoubleSpinBox* spinAlpha = new QDoubleSpinBox(&dlg); spinAlpha->setRange(100, 10000); spinAlpha->setValue(2000);

    QFormLayout* form = new QFormLayout;
    form->addRow("Number of Modes (K):", spinK);
    form->addRow("Bandwidth Constraint (Alpha):", spinAlpha);
    lay->addLayout(form);

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    int K = spinK->value();
    double alpha = spinAlpha->value();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    VmdResult res = compute_vmd(sig, fs, K, alpha, 0.0, 1e-7);
    QApplication::restoreOverrideCursor();

    QWidget* resWindow = new QWidget;
    resWindow->setAttribute(Qt::WA_DeleteOnClose);
    resWindow->setWindowTitle(QString("VMD Modes (K=%1)").arg(K));
    resWindow->resize(850, 750);

    QVBoxLayout* winLayout = new QVBoxLayout(resWindow);
    QScrollArea* sa = new QScrollArea(resWindow);
    sa->setWidgetResizable(true);
    winLayout->addWidget(sa);

    QWidget* content = new QWidget;
    QVBoxLayout* cLayout = new QVBoxLayout(content);

    // 原始信号
    Plot1D* p0 = new Plot1D(content);
    p0->setMinimumHeight(200);
    p0->setData(t, sig, "Original Signal");
    cLayout->addWidget(p0);

    // 各阶模态
    for (int k = 0; k < K; ++k) {
        Plot1D* pk = new Plot1D(content);
        pk->setMinimumHeight(180);
        pk->setData(t, res.modes[k], QString("IMF %1 (Freq: %2 Hz)").arg(k + 1).arg(res.center_freqs[k], 0, 'f', 1));
        cLayout->addWidget(pk);
    }
    cLayout->addStretch();
    sa->setWidget(content);
    resWindow->show();
}

void Plot1D::handleWelchPSD(const std::vector<double>& sig)
{
    double fs = sampleRate();
    std::vector<double> freqs, psd;
    compute_welch_psd(sig, fs, 1024, freqs, psd);

    QWidget* w = new QWidget;
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowTitle("Welch Power Spectral Density");
    w->resize(750, 450);

    QVBoxLayout* l = new QVBoxLayout(w);
    Plot1D* p = new Plot1D(w);
    l->addWidget(p);

    p->yAxis->setScaleType(QCPAxis::stLogarithmic);
    p->setData(freqs, psd, "Welch PSD (dB)");
    p->xAxis->setRange(0, std::min(fs / 2.0, 100.0));
    w->show();
}

void Plot1D::handleHilbertEnvelope(const std::vector<double>& t, const std::vector<double>& sig)
{
    std::vector<double> env = compute_hilbert_envelope(sig);

    QWidget* w = new QWidget;
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowTitle("Hilbert Envelope Analysis");
    w->resize(800, 420);

    QVBoxLayout* l = new QVBoxLayout(w);
    Plot1D* p = new Plot1D(w);
    l->addWidget(p);

    p->setData(t, sig, "Signal & Envelope");
    QCPGraph* gEnv = p->addGraph();
    gEnv->setData(QVector<double>(t.begin(), t.end()), QVector<double>(env.begin(), env.end()));
    gEnv->setPen(QPen(Qt::red, 1.8));
    gEnv->setName("Envelope");
    p->rescaleAxes();
    w->show();
}

void Plot1D::handleFFTSpectrum(const std::vector<double>& sig)
{
    double fs = sampleRate();
    int N = sig.size();

    std::vector<double> detrended = sig;
    double mean = std::accumulate(detrended.begin(), detrended.end(), 0.0) / N;
    for (double& v : detrended) v -= mean;

    Eigen::FFT<double> fft;
    std::vector<std::complex<double>> fftOut;
    fft.fwd(fftOut, detrended);

    int nFreq = N / 2 + 1;
    std::vector<double> freqs(nFreq), mags(nFreq);
    for (int i = 0; i < nFreq; ++i) {
        freqs[i] = i * fs / N;
        double mag = std::abs(fftOut[i]);
        mags[i] = (i == 0 || i == nFreq - 1) ? (mag / N) : (2.0 * mag / N);
    }

    QWidget* w = new QWidget;
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowTitle("FFT Amplitude Spectrum");
    w->resize(750, 420);

    QVBoxLayout* l = new QVBoxLayout(w);
    Plot1D* p = new Plot1D(w);
    l->addWidget(p);

    p->setData(freqs, mags, "Amplitude Spectrum");
    p->xAxis->setRange(0, std::min(fs / 2.0, 80.0));
    w->show();
}

void Plot1D::handleExportData()
{
    if (graphCount() == 0 || !graph(0)) {
        QMessageBox::warning(this, "Export Warning", "No graph data to export.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Data", "Exported_Data.csv",
        "CSV Files (*.csv);;Text Files (*.txt);;All Files (*.*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Cannot write file:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    QString sep = fileName.endsWith(".csv", Qt::CaseInsensitive) ? "," : "\t";

    // 表头
    QString xName = xAxis->label().isEmpty() ? "X" : xAxis->label().remove('\n');
    QString yName = yAxis->label().isEmpty() ? "Y" : yAxis->label().remove('\n');
    out << xName << sep << yName << "\n";

    QCPGraph* g = graph(0);
    auto dataMap = g->data();
    int count = 0;

    auto writeRange = [&](const QCPDataRange& r) {
        auto itBegin = dataMap->begin() + r.begin();
        auto itEnd = dataMap->begin() + r.end();
        if (r.end() > dataMap->size()) itEnd = dataMap->end();
        for (auto it = itBegin; it != itEnd; ++it) {
            out << QString::number(it->key, 'g', 10) << sep
                << QString::number(it->value, 'g', 10) << "\n";
            count++;
        }
        };

    if (!g->selection().isEmpty()) {
        for (const auto& r : g->selection().dataRanges()) writeRange(r);
    }
    else {
        writeRange(QCPDataRange(0, dataMap->size()));
    }

    file.close();
    QToolTip::showText(QCursor::pos(), QString("Exported %1 points successfully.").arg(count));
}