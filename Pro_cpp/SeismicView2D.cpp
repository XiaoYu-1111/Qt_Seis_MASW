#include "Pro_h/SeismicView2D.h"
#include "Pro_h/QCPSeismicWiggle.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox> // 确保包含了此头文件
#include <QLabel>

// 在 SeismicView2D.cpp 顶部增加：
#include "Pro_h/SeismicIO.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

// 全能地震数据显示窗口
QWidget* createSeismicView(
	const std::vector<std::vector<float>>& data,
	const QString& title,
	QWidget* parent, float dt)
{
	if (data.empty() || data[0].empty()) {
		qDebug() << "Empty seismic data";
		return nullptr;
	}

	int nx = data.size();     // Traces
	int nz = data[0].size();  // Samples

	// 1. 创建容器
	QWidget* widget = new QWidget(parent);
	if (!parent) {
		widget->setAttribute(Qt::WA_DeleteOnClose);
		widget->setWindowTitle(title);
		widget->resize(600, 600);
	}

	QVBoxLayout* layout = new QVBoxLayout(widget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// 2. 初始化 QCustomPlot
	QCustomPlot* plot = new QCustomPlot(widget);
	layout->addWidget(plot, 1);
	plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

	// 3. 层1: ColorMap (变密度)
	QCPColorMap* colorMap = new QCPColorMap(plot->xAxis, plot->yAxis);
	colorMap->data()->setSize(nx, nz);
	colorMap->data()->setRange(QCPRange(0, nx), QCPRange(0, nz));

	// 填充数据并计算极值
	double vmin = 1e100, vmax = -1e100;
	for (int x = 0; x < nx; ++x) {
		for (int z = 0; z < nz; ++z) {
			double v = data[x][z];
			if (std::isfinite(v)) {
				vmin = std::min(vmin, v);
				vmax = std::max(vmax, v);
			}
			else {
				v = 0;
			}
			colorMap->data()->setCell(x, z, v);
		}
	}
	// 初始色标范围：自动对称
	double maxAmp = std::max(std::abs(vmin), std::abs(vmax));
	if (maxAmp == 0) maxAmp = 1.0;
	colorMap->setDataRange(QCPRange(-maxAmp * 0.5, maxAmp * 0.5));
	colorMap->setInterpolate(true);

	// 4. 层2: Wiggle (波形图) 
	auto dataPtr = std::make_shared<std::vector<std::vector<float>>>(data);
	QCPSeismicWiggle* wiggle = new QCPSeismicWiggle(plot->xAxis, plot->yAxis);
	wiggle->setData(dataPtr, 1.0f);
	wiggle->setPen(QPen(Qt::black, 1));
	wiggle->setVisible(false);

	// 5. 色标设置
	QCPColorScale* colorScale = new QCPColorScale(plot);
	plot->plotLayout()->addElement(0, 1, colorScale);
	colorScale->setType(QCPAxis::atRight);
	colorScale->axis()->setLabel("Amplitude");
	colorMap->setColorScale(colorScale);

	// 【修改】去掉了这里原有的 setGradient 定义，将其移到了底部控制栏创建之后

	// 6. 坐标轴
	plot->yAxis->setRangeReversed(true); // 深度向下
	plot->xAxis->setLabel("Trace / X");
	plot->yAxis->setLabel("Sample / Z");

	// 美化字体
	QFont tickFont("Segoe UI", 9);
	QFont labelFont("Segoe UI", 10, QFont::Bold);
	plot->xAxis->setTickLabelFont(tickFont); plot->xAxis->setLabelFont(labelFont);
	plot->yAxis->setTickLabelFont(tickFont); plot->yAxis->setLabelFont(labelFont);

	plot->rescaleAxes();
	colorMap->rescaleDataRange(true);
	plot->replot();

	// =========================================================
	// 7. 底部控制栏 (Bottom Bar)
	// =========================================================
	QFrame* bottomBar = new QFrame();
	bottomBar->setFixedHeight(45);
	//bottomBar->setStyleSheet("QFrame { background-color: #f8fafc; border-top: 1px solid #e2e8f0; }");
	bottomBar->setStyleSheet("QFrame { background-color: #0f172a; border-top: 1px solid #334155; }");
	QHBoxLayout* barLayout = new QHBoxLayout(bottomBar);
	barLayout->setContentsMargins(15, 0, 15, 0);
	barLayout->setSpacing(15);

	// [控件] 显示数值
	QLabel* statusLabel = new QLabel("Ready");
	statusLabel->setStyleSheet("color: #64748b; font-family: Consolas; font-size: 12px; border: none;");

	// 【新增】色标控制控件
	QLabel* lblStyle = new QLabel("Color:");
	QComboBox* comboStyle = new QComboBox();
	comboStyle->setFixedWidth(100);
	QStringList cmaps = {
		"Jet", "Terrain", "Seismic", "Grayscale", "Hot", "Cold",
		"Coolwarm", "Viridis", "Plasma", "Turbo", "Inferno", "PuOr"
	};
	comboStyle->addItems(cmaps);
	QCheckBox* chkInvert = new QCheckBox("Inv");
	chkInvert->setToolTip("Invert Colormap");

	// [控件] Image 对比度
	QCheckBox* chkImage = new QCheckBox("Image"); chkImage->setChecked(true);
	QSlider* sliderContrast = new QSlider(Qt::Horizontal); sliderContrast->setRange(1, 200); sliderContrast->setValue(50);
	sliderContrast->setFixedWidth(100);

	//[控件] Wiggle 增益
	QCheckBox* chkWiggle = new QCheckBox("Wiggle");
	QSlider* sliderGain = new QSlider(Qt::Horizontal); sliderGain->setRange(1, 200); sliderGain->setValue(50);
	sliderGain->setFixedWidth(100);

	// =========================================================
	// 【新增】统一的颜色更新逻辑 (取代了原来的 setGradient)
	// =========================================================
	auto updateColorMap = [=]() {
		QString type = comboStyle->currentText();
		QCPColorGradient grad;
		grad.clearColorStops();

		if (type == "Jet" || type == "Jet (Classic)") grad = QCPColorGradient::gpJet;
		else if (type == "Terrain") {
			grad.setColorStopAt(0.0, QColor(0, 60, 170));   grad.setColorStopAt(0.25, QColor(0, 180, 220));
			grad.setColorStopAt(0.45, QColor(240, 240, 120)); grad.setColorStopAt(0.6, QColor(50, 200, 50));
			grad.setColorStopAt(0.8, QColor(120, 100, 60)); grad.setColorStopAt(1.0, QColor(250, 250, 250));
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
			grad.setColorStopAt(0.0, QColor(68, 1, 84)); grad.setColorStopAt(0.2, QColor(72, 35, 116)); grad.setColorStopAt(0.4, QColor(64, 103, 138));
			grad.setColorStopAt(0.6, QColor(53, 183, 121)); grad.setColorStopAt(0.8, QColor(143, 215, 68)); grad.setColorStopAt(1.0, QColor(253, 231, 37));
		}
		else if (type == "Plasma") {
			grad.setColorStopAt(0.0, QColor(13, 8, 135)); grad.setColorStopAt(0.25, QColor(106, 0, 168)); grad.setColorStopAt(0.5, QColor(187, 55, 84));
			grad.setColorStopAt(0.75, QColor(249, 142, 9)); grad.setColorStopAt(1.0, QColor(240, 249, 33));
		}
		else if (type == "Turbo" || type == "Turbo (Better Jet)") {
			grad.setColorStopAt(0.0, QColor(48, 18, 59)); grad.setColorStopAt(0.1, QColor(70, 107, 227)); grad.setColorStopAt(0.2, QColor(40, 187, 235));
			grad.setColorStopAt(0.35, QColor(50, 241, 151)); grad.setColorStopAt(0.5, QColor(164, 252, 60)); grad.setColorStopAt(0.7, QColor(237, 208, 58));
			grad.setColorStopAt(0.85, QColor(253, 128, 40)); grad.setColorStopAt(1.0, QColor(122, 4, 3));
		}
		else if (type == "Inferno") {
			grad.setColorStopAt(0.0, QColor(0, 0, 4)); grad.setColorStopAt(0.25, QColor(87, 16, 109)); grad.setColorStopAt(0.5, QColor(187, 55, 84));
			grad.setColorStopAt(0.75, QColor(249, 142, 9)); grad.setColorStopAt(1.0, QColor(252, 255, 164));
		}
		else if (type == "PuOr" || type == "PuOr (Diverging)") {
			grad.setColorStopAt(0.0, QColor(127, 59, 8)); grad.setColorStopAt(0.25, QColor(253, 184, 99)); grad.setColorStopAt(0.5, Qt::white);
			grad.setColorStopAt(0.75, QColor(178, 171, 210)); grad.setColorStopAt(1.0, QColor(84, 39, 136));
		}

		if (chkInvert->isChecked()) {
			QMap<double, QColor> currentStops = grad.colorStops();
			grad.clearColorStops();
			for (auto it = currentStops.constBegin(); it != currentStops.constEnd(); ++it) {
				grad.setColorStopAt(1.0 - it.key(), it.value());
			}
		}
		colorMap->setGradient(grad);
		plot->replot();
		};

	// 绑定信号
	QObject::connect(comboStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), updateColorMap);
	QObject::connect(chkInvert, &QCheckBox::toggled, updateColorMap);

	// 信号连接
	QObject::connect(chkImage, &QCheckBox::toggled, [=](bool on) {
		colorMap->setVisible(on);
		colorScale->setVisible(on);
		if (on && !plot->plotLayout()->element(0, 1)) plot->plotLayout()->addElement(0, 1, colorScale);
		else if (!on) plot->plotLayout()->take(colorScale);
		plot->replot();
		});
	QObject::connect(sliderContrast, &QSlider::valueChanged, [=](int val) {
		float factor = 2.5f - (val / 100.0f) * 2.0f;
		if (factor < 0.05f) factor = 0.05f;
		colorMap->setDataRange(QCPRange(-maxAmp * factor, maxAmp * factor));
		plot->replot();
		});

	QObject::connect(chkWiggle, &QCheckBox::toggled, wiggle, &QCPSeismicWiggle::setVisible);
	QObject::connect(chkWiggle, &QCheckBox::toggled, [=]() { plot->replot(); });

	float baseGain = wiggle->getGain();
	QObject::connect(sliderGain, &QSlider::valueChanged, [=](int val) {
		wiggle->setGain(baseGain * (val / 50.0f));
		plot->replot();
		});

	// 布局组装
	barLayout->addWidget(statusLabel);
	barLayout->addStretch(); // 弹簧把状态栏挤到左边，其余控件在右侧

	// 【新增】将颜色控件加入布局
	barLayout->addWidget(lblStyle);
	barLayout->addWidget(comboStyle);
	barLayout->addWidget(chkInvert);

	QFrame* vline1 = new QFrame; vline1->setFrameShape(QFrame::VLine); vline1->setStyleSheet("color:#ccc");
	barLayout->addWidget(vline1);

	barLayout->addWidget(chkImage);
	barLayout->addWidget(sliderContrast);

	QFrame* vline2 = new QFrame; vline2->setFrameShape(QFrame::VLine); vline2->setStyleSheet("color:#ccc");
	barLayout->addWidget(vline2);

	barLayout->addWidget(chkWiggle);
	barLayout->addWidget(sliderGain);

	layout->addWidget(bottomBar, 0);

	// 设置默认颜色 (会触发 updateColorMap)
	comboStyle->setCurrentText("Turbo");

	// 8. 鼠标移动显示坐标
	QObject::connect(plot, &QCustomPlot::mouseMove, widget, [=](QMouseEvent* e) {
		double x = plot->xAxis->pixelToCoord(e->pos().x());
		double y = plot->yAxis->pixelToCoord(e->pos().y());
		int xi = std::clamp((int)(x + 0.5), 0, nx - 1);
		int zi = std::clamp((int)(y + 0.5), 0, nz - 1);
		double v = colorMap->data()->cell(xi, zi);
		statusLabel->setText(QString("X: %1  Z: %2  Val: %3").arg(xi).arg(zi).arg(v, 0, 'g', 4));
		});

	// 9. 右键菜单 (Context Menu)
	plot->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(plot, &QCustomPlot::customContextMenuRequested, [=](QPoint pos) {
		QMenu* menu = new QMenu(widget);
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->addAction("Reset View", [=]() {
			plot->rescaleAxes(true);
			plot->replot();
			});
		// 【修改】右键色标选择，直接联动底部的 QComboBox
		QMenu* subCmap = menu->addMenu("Color Map");
		for (int i = 0; i < comboStyle->count(); ++i) {
			QString name = comboStyle->itemText(i);
			subCmap->addAction(name, [=]() {
				// 修改下拉框的值，自动触发 updateColorMap
				comboStyle->setCurrentIndex(i);
				});
		}
		// Wiggle 样式
		QMenu* subWig = menu->addMenu("Wiggle Style");
		subWig->addAction("Line Only", [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmWiggleOnly); plot->replot(); });
		subWig->addAction("Variable Area (Fill)", [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmVariableArea); plot->replot(); });
		subWig->addAction("Both", [=]() { wiggle->setDisplayMode(QCPSeismicWiggle::dmWiggleAndVA); plot->replot(); });

		menu->addSeparator();
		menu->addAction("Auto Color Scale", [=]() {
			colorMap->rescaleDataRange(true);
			plot->replot();
			});
		menu->addAction("Symmetric About Zero", [=]() {
			colorMap->rescaleDataRange(true);
			QCPRange range = colorMap->dataRange();
			double maxVal = std::max(std::abs(range.lower), std::abs(range.upper));
			if (maxVal < 1e-9) maxVal = 1.0;
			colorMap->setDataRange(QCPRange(-maxVal, maxVal));
			plot->replot();
			});
		menu->addSeparator();

		menu->addAction("Save PNG (As Displayed)", [&]() {
			QString f = QFileDialog::getSaveFileName(
				widget,
				"Save Figure",
				"vp.png",
				"PNG (*.png)"
			);
			if (!f.isEmpty())
				plot->savePng(f);   // ← 关键：不传分辨率
			});
		menu->addAction("Save PNG (Publication)", [&]() {

			QString f = QFileDialog::getSaveFileName(
				widget,
				"Save Publication Figure",
				"vp.png",
				"PNG (*.png)"
			);
			if (f.isEmpty()) return;

			// ----------------------------
			// 1. 保存当前样式
			// ----------------------------
			QFont tickFont = plot->xAxis->tickLabelFont();
			QFont labelFont = plot->xAxis->labelFont();
			QPen axisPen = plot->xAxis->basePen();
			QPen tickPen = plot->xAxis->tickPen();

			double factor = 3.0;

			// ----------------------------
			// 2. 放大样式
			// ----------------------------
			auto scaleFont = [&](QFont f) {
				f.setPointSizeF(f.pointSizeF() * factor);
				return f;
				};

			plot->xAxis->setTickLabelFont(scaleFont(tickFont));
			plot->yAxis->setTickLabelFont(scaleFont(tickFont));
			plot->xAxis->setLabelFont(scaleFont(labelFont));
			plot->yAxis->setLabelFont(scaleFont(labelFont));

			QPen p = axisPen; p.setWidthF(p.widthF() * factor);
			plot->xAxis->setBasePen(p);
			plot->yAxis->setBasePen(p);

			p = tickPen; p.setWidthF(p.widthF() * factor);
			plot->xAxis->setTickPen(p);
			plot->yAxis->setTickPen(p);

			plot->replot();

			// ----------------------------
			// 3. 高清导出
			// ----------------------------
			plot->saveRastered(
				f,
				plot->width() * factor,
				plot->height() * factor,
				1.0,
				"PNG",
				50
			);

			// ----------------------------
			// 4. 恢复原始样式
			// ----------------------------
			plot->xAxis->setTickLabelFont(tickFont);
			plot->yAxis->setTickLabelFont(tickFont);
			plot->xAxis->setLabelFont(labelFont);
			plot->yAxis->setLabelFont(labelFont);
			plot->xAxis->setBasePen(axisPen);
			plot->yAxis->setBasePen(axisPen);
			plot->xAxis->setTickPen(tickPen);
			plot->yAxis->setTickPen(tickPen);

			plot->replot();

			});

		menu->addSeparator();

		// =========================================================
		// 【新增】：导出道集为标准 SEGY 文件
		// =========================================================
		QAction* actExportSegy = menu->addAction(QStringLiteral("💾 导出道集为 SEGY (*.sgy)..."));
		QObject::connect(actExportSegy, &QAction::triggered, [=]() {
			QString defaultName = QString("Synthetic_Gather_%1Traces_%2.sgy")
				.arg(nx)
				.arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

			QString fileName = QFileDialog::getSaveFileName(
				widget,
				QStringLiteral("导出地震道集为 SEGY"),
				defaultName,
				QStringLiteral("SEGY 地震数据 (*.sgy *.segy);;所有文件 (*.*)")
			);

			if (fileName.isEmpty()) return;

			// 调用 SeismicIO 写入 SEGY 数据
			SeismicIO::writeSegyFile2D(data, fileName.toStdString(), dt);

			QMessageBox::information(
				widget,
				QStringLiteral("导出成功"),
				QStringLiteral("地震道集已成功导出为 SEGY 格式！\n\n- 总道数: %1 道\n- 每道采样点数: %2 点\n- 采样间隔 dt: %3 ms\n- 保存路径: %4")
				.arg(nx).arg(nz).arg(dt * 1000.0f).arg(fileName)
			);
			});

		menu->popup(plot->mapToGlobal(pos));
	});

	if (!parent) widget->show();
	return widget;
}