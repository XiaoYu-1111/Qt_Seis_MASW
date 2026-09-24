#pragma once

#include "qcustomplot.h"
#include <vector>
#include <memory>

class QCPSeismicWiggle : public QCPAbstractPlottable {
    Q_OBJECT

public:
    enum DisplayMode {
        dmWiggleOnly,       // 仅波形起伏线 (Wiggle Only)
        dmVariableArea,     // 仅正半周变面积填充 (Variable Area - VA)
        dmWiggleAndVA       // 标准地震显示: 波形线 + 正半周填充
    };

    explicit QCPSeismicWiggle(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPSeismicWiggle() override = default;

    // 数据设置 (传入 shared_ptr 保证大数据零拷贝)
    void setData(std::shared_ptr<std::vector<std::vector<float>>> data, float dt);

    // 属性配置
    void setGain(float gain);
    float getGain() const { return m_gain; }

    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_mode; }

    void setFillColor(const QColor& color);
    QColor fillColor() const { return m_fillColor; }

    // QCustomPlot 虚函数规范实现
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth, const QCPRange& inKeyRange = QCPRange()) const override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

protected:
    void draw(QCPPainter* painter) override;

private:
    std::shared_ptr<std::vector<std::vector<float>>> m_data;
    float m_dt = 1.0f;
    float m_gain = 1.0f;
    DisplayMode m_mode = dmWiggleAndVA;
    QColor m_fillColor = Qt::black;
};