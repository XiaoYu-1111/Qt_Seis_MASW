#include "Pro_h/QCPSeismicWiggle.h"
#include <cmath>
#include <algorithm>

QCPSeismicWiggle::QCPSeismicWiggle(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
{
    setPen(QPen(Qt::black, 1));
    setBrush(Qt::black);
}

void QCPSeismicWiggle::setData(std::shared_ptr<std::vector<std::vector<float>>> data, float dt)
{
    m_data = data;
    m_dt = (dt > 0.0f) ? dt : 1.0f; // 防除零保护

    if (!m_data || m_data->empty()) return;

    // 快速采样估算自适应初始增益 (限制波形摆幅在 1 个道间距内)
    float maxVal = 0.0f;
    int step = std::max(1, static_cast<int>(m_data->size() / 50));
    for (size_t i = 0; i < m_data->size(); i += step) {
        for (float v : (*m_data)[i]) {
            maxVal = std::max(maxVal, std::abs(v));
        }
    }

    if (maxVal > 1e-9f) {
        m_gain = 1.0f / maxVal;
    }
}

void QCPSeismicWiggle::setGain(float gain)
{
    m_gain = (gain > 0.0f) ? gain : 1.0f;
}

void QCPSeismicWiggle::setDisplayMode(DisplayMode mode)
{
    m_mode = mode;
}

void QCPSeismicWiggle::setFillColor(const QColor& color)
{
    m_fillColor = color;
    setBrush(color);
}

double QCPSeismicWiggle::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    Q_UNUSED(pos)
        Q_UNUSED(onlySelectable)
        Q_UNUSED(details)
        return -1.0; // 地震剖面通常作为背景视图，关闭图元点选以提升流畅度
}

QCPRange QCPSeismicWiggle::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    Q_UNUSED(inSignDomain)
        if (!m_data || m_data->empty()) {
            foundRange = false;
            return QCPRange();
        }
    foundRange = true;
    return QCPRange(-0.5, static_cast<double>(m_data->size()) - 0.5);
}

QCPRange QCPSeismicWiggle::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain, const QCPRange& inKeyRange) const
{
    Q_UNUSED(inSignDomain)
        Q_UNUSED(inKeyRange)
        if (!m_data || m_data->empty() || (*m_data)[0].empty()) {
            foundRange = false;
            return QCPRange();
        }
    foundRange = true;
    return QCPRange(0.0, static_cast<double>((*m_data)[0].size() - 1) * m_dt);
}

void QCPSeismicWiggle::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    painter->setPen(pen());
    painter->setBrush(m_fillColor);

    // 绘制精致的迷你地震道波形与填充图标
    double midX = rect.center().x();
    double topY = rect.top() + 2;
    double botY = rect.bottom() - 2;

    // 基准线
    painter->drawLine(QLineF(midX, topY, midX, botY));

    // 正半周充填小波包
    QPolygonF iconPoly;
    iconPoly << QPointF(midX, rect.top() + rect.height() * 0.25)
        << QPointF(midX + rect.width() * 0.35, rect.center().y())
        << QPointF(midX, rect.top() + rect.height() * 0.75);
    painter->drawPolygon(iconPoly);
}

// =========================================================
// 核心绘制逻辑 (含精确过零点插值与高性能映射)
// =========================================================
void QCPSeismicWiggle::draw(QCPPainter* painter)
{
    if (!m_data || m_data->empty() || (*m_data)[0].empty()) return;

    QCPAxis* kAxis = keyAxis();
    QCPAxis* vAxis = valueAxis();
    if (!kAxis || !vAxis) return;

    // 1. 视口裁剪 (Frustum Culling)
    QCPRange keyRange = kAxis->range();
    QCPRange valRange = vAxis->range();

    int startTrace = std::clamp(static_cast<int>(std::floor(keyRange.lower - 1.0)), 0, static_cast<int>(m_data->size()));
    int endTrace = std::clamp(static_cast<int>(std::ceil(keyRange.upper + 1.0)), 0, static_cast<int>(m_data->size()));
    if (startTrace >= endTrace) return;

    // 2. LOD 动态步长 (避免百万样点重叠过度绘制)
    double pixelDelta = std::abs(vAxis->coordToPixel(m_dt) - vAxis->coordToPixel(0.0));
    int step = 1;
    if (pixelDelta < 0.5) {
        step = std::clamp(static_cast<int>(0.5 / pixelDelta), 1, 16);
    }

    int nSamples = static_cast<int>((*m_data)[0].size());
    int startSample = std::clamp(static_cast<int>(std::floor(std::min(valRange.lower, valRange.upper) / m_dt)), 0, nSamples);
    int endSample = std::clamp(static_cast<int>(std::ceil(std::max(valRange.lower, valRange.upper) / m_dt)), 0, nSamples);

    startSample = std::max(0, startSample - step);
    endSample = std::min(nSamples, endSample + step);
    if (startSample >= endSample) return;

    painter->setBrush(m_fillColor);

    // 3. 遍历各道进行物理绘制
    for (int i = startTrace; i < endTrace; ++i) {
        const auto& trace = (*m_data)[i];
        if (static_cast<int>(trace.size()) < endSample) continue;

        double xBasePix = kAxis->coordToPixel(i);

        // =====================================================
        // 模式 A: 正半周变面积填充 (含过零点线性精确截断)
        // =====================================================
        if (m_mode == dmVariableArea || m_mode == dmWiggleAndVA) {
            QPolygonF poly;
            poly.reserve((endSample - startSample) / step + 10);

            bool inPositiveLobe = false;
            float prevVal = trace[startSample];
            double prevT = startSample * m_dt;

            for (int j = startSample; j < endSample; j += step) {
                float val = trace[j];
                double t = j * m_dt;

                if (val > 0.0f) {
                    if (!inPositiveLobe && j > startSample) {
                        // 【核心精化】：从负转正，线性插值精确计算过零点进入基线
                        double frac = (val != prevVal) ? (-prevVal / (val - prevVal)) : 0.0;
                        double tZero = prevT + frac * (t - prevT);
                        poly << QPointF(xBasePix, vAxis->coordToPixel(tZero));
                    }
                    double px = kAxis->coordToPixel(i + val * m_gain);
                    double py = vAxis->coordToPixel(t);
                    poly << QPointF(px, py);
                    inPositiveLobe = true;
                }
                else {
                    if (inPositiveLobe) {
                        // 【核心精化】：从正转负，线性插值精确计算过零点退出基线
                        double frac = (prevVal != val) ? (prevVal / (prevVal - val)) : 0.0;
                        double tZero = prevT + frac * (t - prevT);
                        poly << QPointF(xBasePix, vAxis->coordToPixel(tZero));
                        inPositiveLobe = false;
                    }
                }
                prevVal = val;
                prevT = t;
            }

            if (!poly.isEmpty()) {
                // 闭合多边形基线
                poly << QPointF(xBasePix, poly.last().y());
                poly << QPointF(xBasePix, poly.first().y());

                painter->setPen(Qt::NoPen);
                painter->drawPolygon(poly);
            }
        }

        // =====================================================
        // 模式 B: 波形起伏线 (Wiggle)
        // =====================================================
        if (m_mode == dmWiggleOnly || m_mode == dmWiggleAndVA) {
            QVector<QPointF> linePoints;
            linePoints.reserve((endSample - startSample) / step + 2);

            for (int j = startSample; j < endSample; j += step) {
                float val = trace[j];
                double px = kAxis->coordToPixel(i + val * m_gain);
                double py = vAxis->coordToPixel(j * m_dt);
                linePoints.append(QPointF(px, py));
            }

            painter->setPen(pen());
            painter->drawPolyline(linePoints);
        }
    }
}