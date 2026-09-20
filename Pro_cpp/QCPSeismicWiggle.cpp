#include "Pro_h/QCPSeismicWiggle.h"

QCPSeismicWiggle::QCPSeismicWiggle(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
{
    setPen(QPen(Qt::black, 1));
    setBrush(Qt::black); // 默认填充色
}

void QCPSeismicWiggle::setData(std::shared_ptr<std::vector<std::vector<float>>> data, float dt) {
    m_data = data;
    m_dt = dt;

    // 自动计算初始增益
    float max_val = 0.0f;
    if (m_data && !m_data->empty()) {
        int step = std::max(1, (int)m_data->size() / 50); // 快速采样
        for (size_t i = 0; i < m_data->size(); i += step) {
            for (float v : (*m_data)[i]) max_val = std::max(max_val, std::abs(v));
        }
    }
    if (max_val != 0) m_gain = 1.0f / max_val; // 默认振幅限制在1个道间距内

    if (m_data && !m_data->empty()) {
        this->keyAxis()->setRange(-1, m_data->size());
        this->valueAxis()->setRange(0, (*m_data)[0].size() * m_dt);
    }
}

void QCPSeismicWiggle::setGain(float gain) { m_gain = gain; }
void QCPSeismicWiggle::setDisplayMode(DisplayMode mode) { m_mode = mode; }
void QCPSeismicWiggle::setFillColor(QColor color) { m_fillColor = color; setBrush(color); }

// 【补全缺失的实现】
float QCPSeismicWiggle::getGain() const {
    return m_gain;
}
// 空实现，为了编译通过
double QCPSeismicWiggle::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const { return -1.0; }

// 在文件末尾添加以下实现

// 绘制图例图标 (Legend Icon)
void QCPSeismicWiggle::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const {
    // 设置画笔和画刷
    painter->setPen(pen());
    painter->setBrush(brush());

    // 简单绘制：画一条垂直线代表地震道
    // rect 是图例中小图标的矩形区域
    QLineF verticalLine(rect.center().x(), rect.top() + 2, rect.center().x(), rect.bottom() - 2);
    painter->drawLine(verticalLine);

    // 或者绘制一个小波形示意 (可选)
    // QPointF p1(rect.center().x(), rect.top());
    // QPointF p2(rect.right(), rect.center().y());
    // QPointF p3(rect.left(), rect.center().y());
    // QPointF p4(rect.center().x(), rect.bottom());
    // painter->drawLine(p1, p2);
    // painter->drawLine(p2, p3);
    // painter->drawLine(p3, p4);
}

QCPRange QCPSeismicWiggle::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const {
    foundRange = true;
    if (!m_data) return QCPRange();
    return QCPRange(0, m_data->size());
}

QCPRange QCPSeismicWiggle::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain, const QCPRange& inKeyRange) const {
    foundRange = true;
    if (!m_data || m_data->empty()) return QCPRange();
    return QCPRange(0, (*m_data)[0].size() * m_dt);
}

// =========================================================
// 核心绘制函数 (深度优化)
// =========================================================
void QCPSeismicWiggle::draw(QCPPainter* painter) {
    if (!m_data || m_data->empty()) return;

    // 1. 获取屏幕可见范围 (Culling)
    QCPRange keyRange = keyAxis()->range();
    QCPRange valRange = valueAxis()->range();

    int startTrace = std::max(0, (int)floor(keyRange.lower - 1));
    int endTrace = std::min((int)m_data->size(), (int)ceil(keyRange.upper + 1));

    // 如果没有可见道，直接返回
    if (startTrace >= endTrace) return;

    // 2. 计算 LOD 步长 (Performance Optimization)
    // 目标：每像素最多绘制 1-2 个点，避免过度绘制
    double pixelsPerSample = std::abs(coordsToPixels(0, m_dt).y() - coordsToPixels(0, 0).y());
    int step = 1;
    if (pixelsPerSample < 0.5) {
        step = (int)(0.5 / pixelsPerSample); // 如果压缩得很厉害，跳过一些点
        if (step < 1) step = 1;
    }

    int nSamples = (*m_data)[0].size();
    int startSample = std::max(0, (int)floor(valRange.lower / m_dt));
    int endSample = std::min(nSamples, (int)ceil(valRange.upper / m_dt));

    // 扩大一点采样范围以防止边缘断裂
    startSample = std::max(0, startSample - step);
    endSample = std::min(nSamples, endSample + step);

    painter->setPen(pen());
    painter->setBrush(m_fillColor);

    // 3. 遍历每一道进行绘制
    for (int i = startTrace; i < endTrace; ++i) {
        const auto& trace = (*m_data)[i];

        // 预分配内存，减少 realloc
        int estimatedPoints = (endSample - startSample) / step + 2;
        QPolygonF poly;
        poly.reserve(estimatedPoints * 2); // 填充需要更多点

        // --- 模式A: 变密度填充 (Variable Area) ---
        if (m_mode == dmVariableArea || m_mode == dmWiggleAndVA) {
            // 构造填充多边形：正半周填充
            // 算法：沿着波形走，如果值 < 0，强制归零到基准线

            // 起始点 (基准线)
            double x_base_pix = coordsToPixels(i, 0).x();
            double y_start_pix = coordsToPixels(0, startSample * m_dt).y();

            poly << QPointF(x_base_pix, y_start_pix);

            for (int j = startSample; j < endSample; j += step) {
                float val = trace[j];
                float x_amp = (val > 0) ? (val * m_gain) : 0; // 只保留正值，负值钳位到0

                double px = coordsToPixels(i + x_amp, j * m_dt).x();
                double py = coordsToPixels(i + x_amp, j * m_dt).y();
                poly << QPointF(px, py);
            }

            // 闭合到底部基准线
            double y_end_pix = coordsToPixels(0, (endSample - 1) * m_dt).y();
            poly << QPointF(x_base_pix, y_end_pix);

            // 绘制填充
            // 临时去掉 Pen，只画填充，防止边缘有黑线干扰
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(poly);
            painter->setPen(pen()); // 恢复 Pen
        }

        // --- 模式B: 波形线 (Wiggle) ---
        if (m_mode == dmWiggleOnly || m_mode == dmWiggleAndVA) {
            QVector<QPointF> linePoints;
            linePoints.reserve(estimatedPoints);

            for (int j = startSample; j < endSample; j += step) {
                float val = trace[j];
                double px = coordsToPixels(i + val * m_gain, j * m_dt).x();
                double py = coordsToPixels(i + val * m_gain, j * m_dt).y();
                linePoints << QPointF(px, py);
            }
            painter->drawPolyline(linePoints);
        }
    }
}