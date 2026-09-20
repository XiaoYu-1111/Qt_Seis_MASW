#pragma once
#include "qcustomplot.h"
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

class QCPSeismicWiggle : public QCPAbstractPlottable {
    Q_OBJECT

public:
    // 定义显示模式
    enum DisplayMode {
        dmWiggleOnly,       // 仅波形线 (Wiggle)
        dmVariableArea,     // 仅变密度填充 (Variable Area - VA)
        dmWiggleAndVA       // 波形线 + 填充 (标准地震显示)
    };

    explicit QCPSeismicWiggle(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPSeismicWiggle() = default;

    void setData(std::shared_ptr<std::vector<std::vector<float>>> data, float dt);

    // 设置/获取属性
    void setGain(float gain);
    float getGain() const; // 【补全缺失的声明】
    void setDisplayMode(DisplayMode mode);
    void setFillColor(QColor color); // 设置正半周填充颜色

    // 必须实现的虚函数
    virtual double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;
    virtual QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth, const QCPRange& inKeyRange = QCPRange()) const override;

    // --- 必须实现这个虚函数 ---
    virtual void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
protected:
    virtual void draw(QCPPainter* painter) override;

private:
    std::shared_ptr<std::vector<std::vector<float>>> m_data;
    float m_dt = 1.0f;
    float m_gain = 1.0f;
    DisplayMode m_mode = dmWiggleAndVA; // 默认：波形+填充
    QColor m_fillColor = Qt::black;     // 默认填充黑色
};