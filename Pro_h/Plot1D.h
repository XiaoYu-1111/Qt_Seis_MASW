#pragma once

#include "qcustomplot.h"
#include <vector>

namespace Data_show
{
    class Plot1D : public QCustomPlot
    {
        Q_OBJECT

    public:
        explicit Plot1D(QWidget* parent = nullptr);
        ~Plot1D() override = default;

        // --- 数据设置接口 ---
        void setData(const std::vector<float>& vec, const QString& title = QString());
        void setData(const std::vector<double>& x, const std::vector<double>& y, const QString& title = QString());
        void setData(const QVector<double>& x, const QVector<double>& y, const QString& title = QString());

        // 快速更新数据（用于实时刷新，不重建图元、不重新计算坐标轴边距）
        void updateDataOnly(const std::vector<double>& x, const std::vector<double>& y);
        void updateDataOnly(const QVector<double>& x, const QVector<double>& y);

        // 标题与采样率接口
        void setTitle(const QString& title);
        void m_setName(const QString& name); // 保留兼容原代码调用

        void setSampleRate(double fs);
        double sampleRate() const;

        // 重写清空，杜绝野指针
        void clearGraphs();

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;

    private:
        // 核心图层防护与样式
        QCPGraph* ensureMainGraph();
        void applyScientificStyle();
        void setupGraphStyle(QCPGraph* graph, const QString& title, int pointCount);

        // 提取交互选区数据辅助函数
        bool extractSelectedData(std::vector<double>& outT, std::vector<double>& outSig, double& outDt);

        // 右键分析功能子模块（解耦 contextMenuEvent）
        void handleSTransform(const std::vector<double>& t, const std::vector<double>& sig);
        void handleSTFT(const std::vector<double>& t, const std::vector<double>& sig);
        void handleCWT(const std::vector<double>& t, const std::vector<double>& sig);
        void handleVMD(const std::vector<double>& t, const std::vector<double>& sig);
        void handleWelchPSD(const std::vector<double>& sig);
        void handleHilbertEnvelope(const std::vector<double>& t, const std::vector<double>& sig);
        void handleFFTSpectrum(const std::vector<double>& sig);
        void handleExportData();
        void handleSetSampleRate();

    private:
        QCPGraph* m_mainGraph = nullptr;          // 主曲线指针
        QCPTextElement* m_titleElement = nullptr; // 标题元素
        double m_sampleRate = 1000.0;             // 采样率 (Hz)
    };
}